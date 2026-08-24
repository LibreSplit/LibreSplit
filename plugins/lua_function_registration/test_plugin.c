#include "lua.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include <stdio.h>

const abi_version_t abi_version = 0 << 16 | 1; // v0.1
const char plugin_name[] = "Test Plugin";
const char plugin_description[] = "Does nothing, it just exists";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

int do_something_lua(lua_State* L)
{
    if (lua_gettop(L) != 0) {
        printf("No arguments, we're still way too early for that");
        lua_pushnil(L);
        return 1;
    }
    printf("Hello from a Lua C Function");
    lua_pushnumber(L, 1);
    return 1;
}

int plug_init(PlugAPI* api)
{
    api->register_lua_function("sayHello", do_something_lua);
    return 0;
}
