#include "getBaseAddress.h"

#include "../maps/maps.h"
#include "../utils.h"

#include <stdio.h>
#include <string.h>

/**
 * The Lua "getBaseAddress" Auto Splitter function.
 *
 * Takes an optional module name and returns its base address.
 * If the argument passed is absent or nil, defaults to the main module.
 *
 * @param L The Lua State
 */
int getBaseAddress(lua_State* L)
{
    const char* module_name = process.name;
    if (lua_gettop(L) > 0 && !lua_isnil(L, 1)) {
        if (!lua_isstring(L, 1)) {
            printf("[getBaseAddress] Module name must be a string or nil (for the main module)");
            lua_pushnil(L);
            return 1;
        }

        module_name = lua_tostring(L, 1);
    }

    if (!process.pid || !module_name) {
        lua_pushnil(L);
        return 1;
    }

    ProcessMap map;
    if (maps_findMapByName(module_name, &map)) {
        lua_pushinteger(L, (lua_Integer)map.start);
        return 1;
    }

    lua_pushnil(L);
    printf("[getBaseAddress] Cannot find module with name \"%s\"\n", module_name);
    return 1;
}
