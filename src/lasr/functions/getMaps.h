#pragma once

#include "lua.h"
#include "src/lasr/utils.h"

extern ProcessMap* maps_cache;
extern size_t maps_cache_size;

int getMaps(lua_State* L);
