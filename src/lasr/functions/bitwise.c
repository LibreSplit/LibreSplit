#include "bitwise.h"
#include "src/lasr/int64.h"

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

    if (!LUA_ISADDRESS_PRINT(L, 1) || !LUA_ISADDRESS_PRINT(L, 2)) {
        // Arguments are not numbers
        printf("[b_and] Both arguments must be integers");
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);
    uint64_t b = lua_toaddress(L, 2);

    uint64_t result = a & b;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
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

    if (!LUA_ISADDRESS_PRINT(L, 1) || !LUA_ISADDRESS_PRINT(L, 2)) {
        // Arguments are not numbers
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);
    uint64_t b = lua_toaddress(L, 2);

    uint64_t result = a | b;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
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

    if (!LUA_ISADDRESS_PRINT(L, 1) || !LUA_ISADDRESS_PRINT(L, 2)) {
        // Arguments are not numbers
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);
    uint64_t b = lua_toaddress(L, 2);

    uint64_t result = a ^ b;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
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

    if (!LUA_ISADDRESS_PRINT(L, 1)) {
        // Argument is not number
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);

    uint64_t result = ~a;

    if (lua_isnumber(L, 1) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
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

    if (!LUA_ISADDRESS_PRINT(L, 1) || !LUA_ISADDRESS_PRINT(L, 2)) {
        // Arguments are not numbers
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);
    uint64_t b = lua_toaddress(L, 2);

    uint64_t result = a << b;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
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

    if (!LUA_ISADDRESS_PRINT(L, 2) || !LUA_ISADDRESS_PRINT(L, 1)) {
        // Arguments are not numbers
        return 0;
    }

    uint64_t a = lua_toaddress(L, 1);
    uint64_t b = lua_toaddress(L, 2);

    uint64_t result = a >> b;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && result <= LUA_FLOAT_ADDRESS_MAX)
        lua_pushinteger(L, result);
    else
        lua_pushuint64(L, result);
    return 1;
}
