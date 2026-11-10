// Port layer: runs the jailbreak tweak NotEnoughIDsPE (Theos / MobileSubstrate) as a mod
// of the iOS MCPE (v0.16.2) Emulator.
//
// What differs from a jailbroken device, and how this file bridges it:
//
//  * Code writes. On the device the tweak goes through MSHookMemory / vm_protect and
//    sys_icache_invalidate. In the emulator all guest memory is writable and the mod's
//    constructor runs before the game's code is translated by the JIT for the first
//    time, so a plain memcpy is enough (write_memory below).
//
//  * Branch range. The tweak branches from the game's code straight into its own
//    functions and into a "cave" area inside its own __TEXT (B / BL reach +-128 MB; a
//    tweak dylib on iOS is mapped close to the app). The emulator loads mods at
//    0x3xxxxxxxx, far away from the game (0x100000000). So:
//      - the cave area is allocated right after the game image (neid_near_area()),
//      - encB / encBL / inBRange route a target that a branch cannot reach through a
//        veneer in that area ("LDR X16, #8; BR X16; .quad target"). Veneers are only
//        made for the mod's own functions, which the tweak only reaches with calls
//        (BL) or tail calls, where X16 (IP0) may be clobbered as on any call.
//
//  * MSHookFunction -> emu_hook (same 16-byte LDR X16 / BR X16 patch, chains with the
//    emulator's own hooks and other mods).
//
//  * NSLog / NSString logging -> emu_log (emulator.log) and Documents/NotEnoughIDsPE.log
//    in the emulator's home folder.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emu_mod.h"

// ---- libSystem functions provided by the emulator (no SDK headers needed) ----
extern "C" {
size_t malloc_size(const void* ptr);
unsigned int mach_task_self(void);
int vm_allocate(unsigned int task, uint64_t* address, uint64_t size, int flags);
}

// ---- image slide (the emulator loads the game at its preferred address) ----
static inline intptr_t neid_slide() {
    static intptr_t s = (intptr_t)(emu_game_base() - 0x100000000ull);
    return s;
}
#define _dyld_get_image_vmaddr_slide(i) neid_slide()

// ---- logging ----
void neidLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void neidConsoleLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#define NSLog(...) neidConsoleLog(__VA_ARGS__)

// ---- code writes ----
static inline bool write_memory(void* destination, const void* data, size_t size) {
    memcpy(destination, data, size);
    return true;
}
static inline void sys_icache_invalidate(void*, size_t) {}  // written before the JIT translates it

// ---- hooks ----
void neid_hook(void* target, void* replacement, void** original);
#define MSHookFunction(target, replacement, original) neid_hook((void*)(target), (void*)(replacement), (void**)(original))

// ---- branches from the game's code ----
// Area right after the game image: the tweak's cave area (kCaveAreaInsns instructions)
// followed by veneers.
uint64_t neid_near_area();
uint64_t neid_branch_target(uint64_t from, uint64_t to);  // `to`, or a veneer for it
bool neid_branch_reachable(uint64_t from, uint64_t to);

static inline bool neid_in_range(uint64_t from, uint64_t to) {
    int64_t d = (int64_t)(to - from);
    return d >= -(1LL << 27) && d < (1LL << 27);
}
static inline uint32_t encB(uint64_t from, uint64_t to) {
    to = neid_branch_target(from, to);
    return 0x14000000 | (uint32_t)(((int64_t)(to - from) >> 2) & 0x3FFFFFF);
}
static inline uint32_t encBL(uint64_t from, uint64_t to) {
    to = neid_branch_target(from, to);
    return 0x94000000 | (uint32_t)(((int64_t)(to - from) >> 2) & 0x3FFFFFF);
}
static inline bool inBRange(uint64_t from, uint64_t to) { return neid_branch_reachable(from, to); }

// The tweak's cave area was a naked function in its own __TEXT ("&neidCaveArea").
#define neidCaveArea (*(char*)neid_near_area())
