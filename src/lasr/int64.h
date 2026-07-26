#pragma once

#include <lua.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

// Additional Lua types as used by the JIT FFI API.
// Copied from the LuaJIT's internal `lj_obj.h` header.

/** @brief The maximum type ID defined by standard Lua 5.1. */
#define LAST_TT LUA_TTHREAD
/** @brief The type ID for LuaJIT's FFI function prototype objects (`type(x) == 'proto'`). Not used. */
#define LUA_TPROTO (LAST_TT + 1)
/** @brief The type ID for LuaJIT's 'cdata' type, used for `LL` and `ULL` literals as well types created by the `ffi` library. */
#define LUA_TCDATA (LAST_TT + 2)

/**
 * @brief The largest value of a Lua `number` (`double`) that can be reliably used as an address in 64-bit programs.
 *
 * Has the property that `(double)LUA_FLOAT_ADDRESS_MAX != (double)LUA_FLOAT_ADDRESS_MAX + 1` but
 * `(double)LUA_FLOAT_ADDRESS_MAX + 1 == (double)LUA_FLOAT_ADDRESS_MAX + 2`.
 */
#define LUA_FLOAT_ADDRESS_MAX ((1ULL << 53) - 1)

bool lua_isint64(lua_State* L, int idx);
uint64_t lua_toint64(lua_State* L, int idx);
void lua_pushsint64(lua_State* L, int64_t n);
void lua_pushuint64(lua_State* L, uint64_t n);

bool lua_isaddress(lua_State* L, int idx);
bool lua_isaddress_print(lua_State* L, int idx, const char* function);
uintptr_t lua_toaddress(lua_State* L, int idx);

/** Convenience macro for invoking `lua_isaddress_print()` passing the current function name. */
#define LUA_ISADDRESS_PRINT(L, idx) lua_isaddress_print((L), (idx), __func__)
