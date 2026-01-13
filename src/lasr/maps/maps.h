#pragma once

#include "src/lasr/utils.h"

#define MAPS_CACHE_BLOCK_SIZE 256

typedef struct MapsBlock {
    struct MapsBlock* next;
    size_t used;
    ProcessMap entries[MAPS_CACHE_BLOCK_SIZE];
} MapsBlock;

extern int maps_cache_cycles;

size_t maps_getAll(void);
void maps_clearCache(void);
bool maps_findMapByName(const char* name, ProcessMap* out_map);
