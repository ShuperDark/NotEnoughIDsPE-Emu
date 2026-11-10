// NotEnoughIDsPE API for mods of the iOS MCPE (v0.16.2) Emulator.
//
// The functions are exported by NotEnoughIDsPE.dylib (extern "C", prefix NEID_) and are
// the same as the jailbreak tweak's (see README.md of the tweak for each function).
// A mod looks them up with dlsym at the time it uses them - not in its constructor:
// mods are loaded in file name order, so NotEnoughIDsPE may not be loaded yet then.
//
//     #include "NotEnoughIDsPE.h"
//     ...
//     if (!neid::load()) return;          // e.g. in a Block::initBlocks hook
//     neid::registerBlock16(65535, block, 3);
#pragma once

#include <dlfcn.h>
#include <stdint.h>

struct Block;
struct Item;
struct BlockSource;
struct BlockPos { int x, y, z; };

namespace neid {

#define NEID_API_FUNCTIONS(X)                                                                                 \
    X(int, apiStage, (void))                                                                                  \
    X(bool, registerBlock16, (uint16_t id, Block* block, uint8_t graphicsFrom))                               \
    X(bool, setBlockTextures, (uint16_t id, const char* up, const char* down, const char* north,             \
                               const char* south, const char* west, const char* east, uint8_t soundFrom))     \
    X(bool, setBlockDestroyTime, (uint16_t id, float time))                                                   \
    X(bool, setBlockExplosionResistance, (uint16_t id, float resistance))                                     \
    X(bool, setBlockFriction, (uint16_t id, float friction))                                                  \
    X(bool, setBlockCreativeCategory, (uint16_t id, int category))                                            \
    X(bool, setBlockLight, (uint16_t id, int emission, int lightBlock))                                       \
    X(bool, setBlockTicking, (uint16_t id, bool ticking))                                                     \
    X(bool, setBlockFallback, (uint16_t id, uint8_t vanillaId))                                               \
    X(bool, setBlockFlammable, (uint16_t id, int flameOdds, int burnOdds))                                    \
    X(bool, setBlockShape, (uint16_t id, int shape))                                                          \
    X(Block*, cloneBlock, (uint16_t id, uint8_t vanillaId, const char* name))                                 \
    X(bool, setBlockLitPair, (uint16_t unlitId, uint16_t litId))                                              \
    X(bool, setBlockColor, (uint16_t id, uint32_t rgb))                                                       \
    X(void*, createFeature, (int type, uint16_t id, uint8_t data, int param))                                 \
    X(void*, createTreeFeature, (int kind, uint16_t logId, uint16_t leavesId))                                \
    X(bool, addFeature, (void* feature, int count, int minY, int maxY))                                       \
    X(bool, addFeatureIn, (void* feature, int dimension, int count, int minY, int maxY))                      \
    X(bool, placeFeature, (void* feature, BlockSource* region, const BlockPos* pos, void* random))            \
    X(Block*, getBlock16, (uint16_t id))                                                                      \
    X(Item*, registerBlockItem16, (uint16_t blockId, const char* name))                                       \
    X(bool, setBlock, (BlockSource* region, const BlockPos* pos, uint16_t id, uint8_t data, int flags))      \
    X(uint16_t, getBlockID, (BlockSource* region, const BlockPos* pos))

#define NEID_DECLARE(ret, name, args) inline ret(*name) args = nullptr;
NEID_API_FUNCTIONS(NEID_DECLARE)
#undef NEID_DECLARE

// Looks every function up (once). True when NotEnoughIDsPE is loaded and implements
// API stage 7 (every function above).
inline bool load() {
    if (registerBlock16 && apiStage) return apiStage() >= 7;
#define NEID_LOAD(ret, name, args) name = (ret(*) args)dlsym(RTLD_DEFAULT, "NEID_" #name);
    NEID_API_FUNCTIONS(NEID_LOAD)
#undef NEID_LOAD
    return registerBlock16 && apiStage && apiStage() >= 7;
}

// Item::mItems after NotEnoughIDsPE: the old table (0x1012AE238, 512 entries) holds the
// pointer to the new one (65536 entries) in its entry 1. Never write the old table.
inline Item** itemTable(uint64_t gameBase) { return ((Item***)(gameBase - 0x100000000ull + 0x1012AE238ull))[1]; }

// NEID_createFeature types and NEID_createTreeFeature kinds
enum FeatureType { kOre = 0, kFlower = 1, kTallGrass = 2, kSpring = 3, kHellSpring = 4, kBlockBlob = 5, kSand = 6, kLake = 7 };
enum TreeKind {
    kOak = 0, kBirch = 1, kTallBirch = 2, kSpruce = 3, kPine = 4, kMegaPine = 5, kMegaSpruce = 6,
    kFancyOak = 7, kSwampOak = 8, kJungle = 9, kMegaJungle = 10, kDarkOak = 11, kAcacia = 12
};

}  // namespace neid
