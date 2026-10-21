#include "time.h"

#include <errno.h>
#include <lauxlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief Returns a monotonic timestamp as signed integer microseconds.
 *
 * @param L The curreent lua state.
 * @return int 1 on success or luaL_error otherwise.
 */
int time_us(lua_State* L)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return luaL_error(L, "cannot read monotonic clock: %s", strerror(errno));
    }

    lua_Integer microseconds = now.tv_nsec / 1000;
    if (now.tv_sec < 0 || now.tv_sec > (LUA_MAXINTEGER - microseconds) / 1000000) {
        return luaL_error(L, "monotonic timestamp is outside the signed 64-bit microsecond range");
    }

    lua_pushinteger(L, (lua_Integer)now.tv_sec * 1000000 + microseconds);
    return 1;
}

/**
 * @brief Returns t2 - t1 in microseconds without overflowing the signed result.
 *
 * @param L The curreent lua state.
 * @return int 1 on success or luaL_error otherwise.
 */
int difftime_us(lua_State* L)
{
    lua_Integer t2 = luaL_checkinteger(L, 1);
    lua_Integer t1 = luaL_checkinteger(L, 2);
    if ((t1 > 0 && t2 < LUA_MININTEGER + t1) || (t1 < 0 && t2 > LUA_MAXINTEGER + t1)) {
        return luaL_error(L, "time difference is outside the signed 64-bit microsecond range");
    }

    lua_pushinteger(L, t2 - t1);
    return 1;
}