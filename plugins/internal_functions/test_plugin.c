#include "lasr/functions/getMaps.h"
#include "lasr/maps/maps.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include <stdio.h>

const abi_version_t abi_version = 1; // v0.1
const char plugin_name[] = "Test Plugin 3";
const char plugin_description[] = "Does something, wraps the getMaps function to demonstrate calling LS functions";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

/**
 * Just a clone of getMaps, to demonstrate calling functions from a
 * plugin
 */
int getmaps_external(lua_State* L)
{
    printf("You are now using getMaps from a plugin");
    if (lua_gettop(L) != 0) {
        printf("No arguments, we're still way too early for that");
        lua_pushnil(L);
        return 1;
    }
    if (maps_cache == NULL) {
        maps_cache_size = maps_getAll();
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

int plug_init(PlugAPI* api)
{
    api->register_lua_function("getMapsExternal", getmaps_external);
    return 0;
}
