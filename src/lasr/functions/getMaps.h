#include "lua.h"
#include "src/lasr/utils.h"

extern ProcessMap* maps_cache;
extern size_t maps_cache_size;

int get_maps(lua_State* L);
