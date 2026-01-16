#include "getMaps.h"

#include "../maps/maps.h"

#include <stdio.h>

/**
 * Dumps the Maps cache into a lua array
 *
 * @param L The lua stack
 */
int getMaps(lua_State* L)
{
    if (lua_gettop(L) != 0) {
        printf("[getMaps] No arguments accepted");
        lua_pushnil(L);
        return 1;
    }
    if (maps_cache == NULL) {
        // Whoops, cache is not filled yet, let's do it now
        maps_getAll();
    }
    // Create a table, in "array mode", maps_cache_size big
    lua_createtable(L, maps_cache_size, 0);
    // Stack: array
    for (uint32_t i = 0; i < maps_cache_size; i++) {
        ProcessMap map = maps_cache[i];
        // Create a new table with 4 non-array fields
        lua_createtable(L, 0, 4);
        // Stack: array, table
        // Push "name" onto the stack
        lua_pushstring(L, map.name);
        // Stack: array, table, value
        lua_setfield(L, -2, "name");
        // Stack: array, table
        // Push "start"
        lua_pushnumber(L, map.start);
        // Stack: array, table, value
        lua_setfield(L, -2, "start");
        // Stack: array, table
        // Push "end"
        lua_pushnumber(L, map.end);
        lua_setfield(L, -2, "end");
        // Push "size"
        lua_pushnumber(L, map.size);
        lua_setfield(L, -2, "size");

        // Stack: Array, Table, [TopOfStack]
        // Assign the table on top of the stack as the i-th element of
        // the table 2 values below the top of stack
        lua_rawseti(L, -2, i + 1);
        // Stack: Array
    }
    return 1;
}
