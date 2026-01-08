#include "shallow_copy_tbl.h"

#include <stdio.h>

/**
 * The Lua "shallow copy table" function.
 *
 * A utility function to quickly shallow-copy tables in auto splitters
 * without having to copy values one-by one.
 *
 * @param L The Lua state
 */
int shallow_copy_tbl(lua_State* L)
{
    if (lua_gettop(L) == 0 || !lua_istable(L, 1)) {
        // Not a table, print a message and push nil to the stack
        printf("[shallow_copy_tbl] Argument is not a table or no argument passed.");
        lua_pushnil(L);
        return 1;
    }

    if (lua_gettop(L) > 1) {
        // Too many arguments
        printf("[shallow_copy_tbl] Too many arguments passed, only pass a single table");
        lua_pushnil(L);
        return 1;
    }

    // Argument is a table, begin the shallow copy process
    // Create a new table
    lua_newtable(L);
    // Stack: original_table, new_table
    int destination_index = lua_gettop(L);
    // Push a nil to the stack to bootstrap lua_next's key
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        // Stack should be original_table, new_table, key, value
        // Old key is popped, Key is at -2, Value is at -1
        // Copy the key
        lua_pushvalue(L, -2);
        // Stack is now original_table, new_table, key, value, key
        // We need to swap the key and value to be able to use settable
        lua_insert(L, -2);
        // Now the stack is original_table, new_table, key, key, value
        lua_settable(L, destination_index);
        // After this, the new key and value should be popped from the stack
        // Stack: original_table, new_table, key, ready for next loop
    }
    // By the end, the stack should be original_table, new_table
    // so the new table should be already on top of the stack
    return 1;
}