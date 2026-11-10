// Port layer implementation (see neid_port.h).
#include "neid_port.h"

#include <time.h>

#include <map>
#include <string>

// ============================================================================
//  Logging
// ============================================================================
namespace {

FILE* logFile() {
    static FILE* fp = [] {
        // Documents/NotEnoughIDsPE.log in the game's home folder, like on the device
        // (<emulator data>\home\Documents). Recreated at every start.
        const char* home = getenv("HOME");
        std::string path = std::string(home ? home : ".") + "/Documents/NotEnoughIDsPE.log";
        return fopen(path.c_str(), "w");
    }();
    return fp;
}

void vlog(bool toFile, const char* fmt, va_list ap) {
    char msg[2048];
    vsnprintf(msg, sizeof msg, fmt, ap);
    char line[2100];
    snprintf(line, sizeof line, "[NotEnoughIDsPE] %s", msg);
    emu_log(line);
    if (!toFile) return;
    if (FILE* fp = logFile()) {
        time_t t = time(nullptr);
        char stamp[32];
        strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&t));
        fprintf(fp, "%s %s\n", stamp, msg);
        fflush(fp);
    }
}

}  // namespace

void neidLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(true, fmt, ap);
    va_end(ap);
}

// The tweak's NSLog lines (they already start with "[NotEnoughIDsPE] ").
void neidConsoleLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char msg[2048];
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    emu_log(msg);
}

// ============================================================================
//  Hooks
// ============================================================================
void neid_hook(void* target, void* replacement, void** original) {
    uint64_t orig = 0;
    int r = emu_hook((uint64_t)target, (uint64_t)replacement, original ? &orig : nullptr);
    if (r != 0) {
        neidLog("hook of %llx failed (%d)", (unsigned long long)(uint64_t)target, r);
        return;
    }
    if (original) *original = (void*)orig;
}

// ============================================================================
//  Area next to the game image: caves + veneers
// ============================================================================
namespace {

constexpr uint64_t kCaveBytes = 8192 * 4;     // the tweak's kCaveAreaInsns
constexpr uint64_t kVeneerBytes = 0x10000;    // 4096 veneers of 16 bytes
constexpr uint64_t kAreaBytes = kCaveBytes + kVeneerBytes;

uint64_t gGameStart = 0, gGameEnd = 0;        // the game's mapped segments
uint64_t gArea = 0;
uint64_t gVeneerUsed = 0;
std::map<uint64_t, uint64_t>* gVeneers = nullptr;

// End of the game image's segments (from its Mach-O load commands).
void findGameRange() {
    gGameStart = emu_game_base();
    const uint8_t* mh = (const uint8_t*)gGameStart;
    uint32_t ncmds = *(const uint32_t*)(mh + 16);
    const uint8_t* lc = mh + 32;
    uint64_t end = gGameStart;
    for (uint32_t i = 0; i < ncmds; i++) {
        uint32_t cmd = *(const uint32_t*)lc, size = *(const uint32_t*)(lc + 4);
        if (cmd == 0x19) {  // LC_SEGMENT_64
            uint64_t vmaddr = *(const uint64_t*)(lc + 24) + neid_slide();
            uint64_t vmsize = *(const uint64_t*)(lc + 32);
            if (vmaddr >= gGameStart && vmaddr + vmsize > end) end = vmaddr + vmsize;  // skips __PAGEZERO
        }
        lc += size;
    }
    gGameEnd = end;
}

}  // namespace

uint64_t neid_near_area() {
    if (gArea) return gArea;
    findGameRange();
    // First free 64 KB-aligned range after the game (VirtualAlloc granularity) that
    // every instruction of the game's __TEXT can still reach with B / BL.
    for (uint64_t a = (gGameEnd + 0xFFFF) & ~0xFFFFull; a + kAreaBytes - gGameStart < (1ull << 27); a += 0x10000) {
        uint64_t addr = a;
        if (vm_allocate(mach_task_self(), &addr, kAreaBytes, 0 /* VM_FLAGS_FIXED */) == 0 && addr == a) {
            gArea = a;
            break;
        }
    }
    if (!gArea) {
        neidLog("no free memory next to the game image; nothing can be patched");
        abort();
    }
    // like the tweak's area: BRK #0x4E49 everywhere
    for (uint64_t k = 0; k < kAreaBytes / 4; k++) ((uint32_t*)gArea)[k] = 0xD4200000 | (0x4E49 << 5);
    neidLog("port: game %llx-%llx, cave and veneer area at %llx", (unsigned long long)gGameStart,
            (unsigned long long)gGameEnd, (unsigned long long)gArea);
    return gArea;
}

namespace {

bool isNearCode(uint64_t a) {
    neid_near_area();
    return (a >= gGameStart && a < gGameEnd) || (a >= gArea && a < gArea + kAreaBytes);
}

uint64_t veneerFor(uint64_t to) {
    if (!gVeneers) gVeneers = new std::map<uint64_t, uint64_t>();
    auto it = gVeneers->find(to);
    if (it != gVeneers->end()) return it->second;
    if (gVeneerUsed + 16 > kVeneerBytes) {
        neidLog("port: veneer area full");
        return 0;
    }
    uint64_t v = gArea + kCaveBytes + gVeneerUsed;
    gVeneerUsed += 16;
    uint32_t* p = (uint32_t*)v;
    p[0] = 0x58000050;  // LDR X16, #8
    p[1] = 0xD61F0200;  // BR X16
    *(uint64_t*)(p + 2) = to;
    (*gVeneers)[to] = v;
    return v;
}

}  // namespace

uint64_t neid_branch_target(uint64_t from, uint64_t to) {
    if (neid_in_range(from, to) || isNearCode(to)) return to;
    uint64_t v = veneerFor(to);
    return v ? v : to;
}

bool neid_branch_reachable(uint64_t from, uint64_t to) {
    if (neid_in_range(from, to)) return true;
    if (isNearCode(to)) return false;
    // a veneer in the area next to the game
    return neid_in_range(from, neid_near_area() + kCaveBytes) && neid_in_range(from, neid_near_area() + kAreaBytes);
}
