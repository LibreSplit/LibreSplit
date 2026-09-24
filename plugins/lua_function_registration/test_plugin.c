#include "lua.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include <stdio.h>

/* =======================================
 * Metadata section
 * ---------------------------------------
 * Here we write all the metadata of the plugin, as well as
 * the supported ABI version
 */
const abi_version_t abi_version = 0 << 16 | 1; // v0.1
const char plugin_name[] = "Test Plugin";
const char plugin_description[] = "Does nothing, it just exists";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

// We create a simple Lua function handler
int do_something_lua(lua_State* L)
{
    // We make sure our function accepts no arguments.
    if (lua_gettop(L) != 0) {
        printf("No arguments, we're still way too early for that");
        // If arguments are passed to this function, we just return nil
        lua_pushnil(L);
        return 1;
    }
    // If the function is called correctly, we print something in C...
    printf("Hello from a Lua C Function");
    // And return "42" as a response to Lua
    // so the lua code "print(sayHello())" will print "42"
    // (after the C code prints "Hello from a Lua C function")
    lua_pushnumber(L, 42);
    // Since we pushed a value back to lua, we return 1
    return 1;
}

int plug_init(PlugAPI* api)
{
    // We register the do_something_lua C function under the name "sayHello"
    // in lua.
    api->register_lua_function("sayHello", do_something_lua);
    return 0;
}
