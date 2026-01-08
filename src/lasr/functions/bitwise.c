#include "bitwise.h"

#include <stdio.h>

/**
 * Performs a binary "and" operation between two integers.
 *
 * @param L the Lua state.
 */
int b_and(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        // Too few/many arguments passed
        printf("[b_and] Two arguments are required.");
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Arguments are not numbers
        printf("[b_and] Both arguments must be integers");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);
    lua_Integer b = lua_tointeger(L, 2);

    lua_Integer result = a & b;

    lua_pushinteger(L, result);
    return 1;
}

/**
 * Performs a binary "or" operation between two integers.
 *
 * @param L the Lua state.
 */
int b_or(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        // Too few/many arguments passed
        printf("[b_or] Two arguments are required.");
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Arguments are not numbers
        printf("[b_or] Both arguments must be integers");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);
    lua_Integer b = lua_tointeger(L, 2);

    lua_Integer result = a | b;

    lua_pushinteger(L, result);
    return 1;
}

/**
 * Performs a binary "xor" operation between two integers.
 *
 * @param L the Lua state.
 */
int b_xor(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        // Too few/many arguments passed
        printf("[b_xor] Two arguments are required.");
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Arguments are not numbers
        printf("[b_xor] Both arguments must be integers");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);
    lua_Integer b = lua_tointeger(L, 2);

    lua_Integer result = a ^ b;

    lua_pushinteger(L, result);
    return 1;
}

/**
 * Performs a binary "not" operation on a single integer.
 *
 * @param L the Lua state.
 */
int b_not(lua_State* L)
{
    if (lua_gettop(L) != 1) {
        // Too few/many arguments passed
        printf("[b_not] One argument is required.");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        // Argument is not number
        printf("[b_not] The argument must be an integer");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);

    lua_Integer result = ~a;

    lua_pushinteger(L, result);
    return 1;
}

/**
 * Performs a binary "left shift" operation on an integer.
 *
 * @param L the Lua state.
 */
int b_lshift(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        // Too few/many arguments passed
        printf("[b_lshift] Two arguments are required.");
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Arguments are not numbers
        printf("[b_lshift] Both arguments must be integers");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);
    lua_Integer b = lua_tointeger(L, 2);

    lua_Integer result = a << b;

    lua_pushinteger(L, result);
    return 1;
}

/**
 * Performs a binary "right shift" operation on an integer.
 *
 * @param L the Lua state.
 */
int b_rshift(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        // Too few/many arguments passed
        printf("[b_rshift] Two arguments are required.");
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Arguments are not numbers
        printf("[b_rshift] Both arguments must be integers");
        return 0;
    }

    lua_Integer a = lua_tointeger(L, 1);
    lua_Integer b = lua_tointeger(L, 2);

    lua_Integer result = a >> b;

    lua_pushinteger(L, result);
    return 1;
}
