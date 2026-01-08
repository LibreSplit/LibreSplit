#include "getBaseAddress.h"

#include "../utils.h"

#include <stdio.h>

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
    uintptr_t address;
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        // No arguments passed or first argument is nil, search for process base address
        address = find_base_address(NULL);
        lua_pushnumber(L, address);
        return 1;
    }
    if (lua_isstring(L, 1)) {
        // Module name passed, search for its base address
        const char* module_name = lua_tostring(L, 1);
        address = find_base_address(module_name);
        lua_pushnumber(L, address);
        return 1;
    }
    printf("Cannot search for base address: module name must be a string or nil (for main module)");
    return 0;
}
