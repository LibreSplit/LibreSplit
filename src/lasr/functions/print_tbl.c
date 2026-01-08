
#include "print_tbl.h"

#include "../utils.h"

#include <stdio.h>

/**
 * The Lua "inspect and print table" command
 *
 * Takes a simple, non-nested table and prints it into console.
 * Useful for debugging purposes.
 *
 * Keys with "nil" as values won't be printed, cause on Lua
 * deleting a key-value pair from a table is done by assigning "nil" as value.
 *
 * @param L The Lua state
 */
int print_tbl(lua_State* L)
{
    if (lua_gettop(L) == 0 || !lua_istable(L, 1)) {
        // Not a table, print a message and push nil to the stack
        printf("[print_tbl] Argument is not a table or no argument passed.");
        return 0;
    }

    if (lua_gettop(L) > 1) {
        // Too many arguments
        printf("[shallow_copy_tbl] Too many arguments passed, only pass a single table");
        return 0;
    }

    // Argument is a table, begin the printing process
    // Push a nil to the stack to bootstrap lua_next's key
    lua_pushnil(L);
    // Stack: table, nil
    while (lua_next(L, 1) != 0) {
        // Old key is popped, Key is at -2, Value is at -1
        // Push the key to the stack to avoid confusing lua_next with
        // the call to lua_tostring (which can change the stack values)
        lua_pushvalue(L, -2);
        // Stack: table, key, value, key_copy
        const char* key_str = value_to_c_string(L, -1);
        const char* value_str = value_to_c_string(L, -2);
        printf("%s: %s\n", key_str, value_str);
        // Pop all except the original key
        lua_pop(L, 2);
    }
    return 0;
}
