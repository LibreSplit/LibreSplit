#pragma once

#include "lasr/utils.h"
#include <lua.h>

extern ProcessMap* maps_cache;
extern size_t maps_cache_size;

int getMaps(lua_State* L);
