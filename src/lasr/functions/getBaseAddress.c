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
    char module_name[PATH_MAX];

    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        strncpy(module_name, process.name, sizeof(module_name) - 1);
    } else if (lua_isstring(L, 1)) {
        const char* str = lua_tostring(L, 1);
        strncpy(module_name, str, sizeof(module_name) - 1);
    } else {
        // Called with invalid non-string parameter
        printf("[getBaseAddress] Module name must be a string or nil (for the main module)");
        lua_pushnil(L);
        return 1;
    }

    // Module name passed, search for its base address
    ProcessMap map;
    bool found = maps_findMapByName(module_name, &map);
    if (found) {
        lua_pushnumber(L, map.start);
        return 1;
    }
    printf("[getBaseAddress] Cannot search for base address: module name must be a string or nil (for main module)");
    return 0;
}
