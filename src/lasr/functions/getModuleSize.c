#include "getModuleSize.h"

#include "../utils.h"
#include "src/lasr/maps/maps.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * The lua getModuleSize() function.
 *
 * FIXME: Size is taken as an unsigned long, which may
 *    ^   exceed the size of lua_Integer
 *
 * @param L The lua state
 */
int getModuleSize(lua_State* L)
{
    char module_name[PATH_MAX];

    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        strncpy(module_name, process.name, sizeof(module_name) - 1);
    } else if (lua_isstring(L, 1)) {
        const char* str = lua_tostring(L, 1);
        strncpy(module_name, str, sizeof(module_name) - 1);
    } else {
        // Called with invalid non-string parameter
        printf("[getModuleSize] Module name must be a string or nil (for the main module)");
        lua_pushnil(L);
        return 1;
    }

    ProcessMap map;
    const bool found = maps_findMapByName(module_name, &map);
    if (found) {
        lua_pushinteger(L, (lua_Integer)map.size);
        return 1;
    }
    lua_pushnil(L);
    printf("[getModuleSize] Cannot find module with name \"%s\"", module_name);
    return 1;
}