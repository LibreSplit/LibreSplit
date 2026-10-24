#include "getModuleSize.h"

#include "../maps/maps.h"
#include "../utils.h"

#include <stdio.h>

/**
 * The Lua getModuleSize() function.
 *
 * FIXME: Size is taken as an unsigned long, which may
 *    ^   exceed the size of lua_Integer
 *
 * @param L The Lua state
 */
int getModuleSize(lua_State* L)
{
    const char* module_name = process.name;

    if (lua_gettop(L) > 0 && !lua_isnil(L, 1)) {
        if (!lua_isstring(L, 1)) {
            // Called with invalid non-string parameter
            printf("[getModuleSize] Module name must be a string or nil (for the main module)");
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
        lua_pushinteger(L, (lua_Integer)map.size);
        return 1;
    }

    lua_pushnil(L);
    printf("[getModuleSize] Cannot find module with name \"%s\"\n", module_name);
    return 1;
}
