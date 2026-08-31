/**
 * @file int64.c
 * @brief Convenience functions for using LuaJIT's FFI types for 64-bit integer support.
 *
 * Due to the way floating-point numbers work, (see [here](https://w.wiki/5xCp) for details)
 * `double`s with values of 2**53 and above will be rounded to the nearest multiple of some power of
 * two, so they cannot reliably be used as offsets into a 64-bit address space. This is the cause of
 * [issue 308](https://github.com/LibreSplit/LibreSplit/issues/308), making it impossible to read
 * data placed at large addresses.
 *
 * LuaJIT supports 64-bit signed and unsigned types as part of its FFI support, with a custom syntax
 * for integer literals syntax (`42LL`, `0xffULL`). These values have a `type()` of `"cdata"` and a
 * `lua_type()` value of #LUA_TCDATA (10). They natively support arithmetic and comparisons with
 * standard Lua `number`s as well as being passed to `tostring()`, `print()`, `string.format()` and
 * the `bit.*` functions, but cannot be concatenated with `..`. We add support for string
 * concatenation in setup_int64_overloads().
 */

#include "int64.h"
#include "utils.h"

#include <lauxlib.h>
#include <lua.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Determine whether a value on the Lua stack represents a LuaJIT 64-bit integer.
 *
 * Cannot distinguish integers from other `cdata` types created by the Lua `ffi` library. This is
 * fine as scripts can't currently create other FFI types (except `complex` with the `I` suffix but
 * that's 16 bytes), but if in future they can we should insert a check equivalent to the following Lua
 * code to avoid an out-of-bounds read in lua_toint64().
 * ```lua
 * local ffi = require('ffi')
 * assert(ffi.typeof(v) == 'ctype<int64_t>' || ffi.typeof(v) == 'ctype<uint64_t>')
 * ```
 *
 * @param L The Lua state.
 * @param idx An index into the Lua stack.
 * @return True if the stack element at `idx` is a `<int>LL` or `<int>ULL` integer.
 */
bool lua_isint64(lua_State* L, int idx)
{
    return lua_type(L, idx) == LUA_TCDATA;
}

/**
 * @brief Fetch a LuaJIT 64-bit integer from the stack.
 * @param L The Lua state.
 * @param idx An index into the Lua stack;
 * @return The 64-bit integer at `idx`, or `0` in the event of an error.
 */
uint64_t lua_toint64(lua_State* L, int idx)
{
    uint64_t* p = (uint64_t*)lua_topointer(L, idx);
    if (!lua_isint64(L, idx) || !p)
        return 0;

    // XXX: This may read out-of-bounds if the `cdata` item at `idx` is smaller than 8 bytes.
    // This is not possible presently, as without the `ffi` module the only way to produce a `cdata`
    // value is with the `<int>LL`, `<int>ULL` or `<float>I` syntax.
    return *p;
}

/**
 * @brief Push a value to the Lua stack as a LuaJIT FFI 64-bit signed integer.
 * @param L The Lua state.
 * @param n A 64-bit integer.
 */
void lua_pushsint64(lua_State* L, int64_t n)
{
    char code[28]; // strlen("return 0xffffffffffffffffLL") + 1
    snprintf(code, sizeof(code), "return 0x%lxLL", n);
    (void)luaL_dostring(L, code);
}

/**
 * @brief Push a value to the Lua stack as a LuaJIT FFI 64-bit unsigned integer.
 * @param L The Lua state.
 * @param n A 64-bit integer.
 */
void lua_pushuint64(lua_State* L, uint64_t n)
{
    char code[29]; // strlen("return 0xffffffffffffffffULL") + 1
    snprintf(code, sizeof(code), "return 0x%lxULL", n);
    (void)luaL_dostring(L, code);
}

/**
 * @brief Return whether a Lua value can be interpreted as a 64-bit address.
 *
 * Equivalent to invoking lua_isaddress_warn() with a `NULL` value for `function`.
 *
 * @param L The Lua state.
 * @param idx An index into the Lua stack.
 * @return True if the element in the Lua stack at `idx` represents a LuaJIT 64-bit integer, or a
 * floating-point number small enough to be converted into one.
 */
bool lua_isaddress(lua_State* L, int idx)
{
    return lua_isaddress_print(L, idx, NULL);
}

/**
 * @brief Return whether a Lua value can be interpreted as a 64-bit address, and print an error if it can't.
 *
 * For a more convenient interface, see LUA_ISADDRESS(), which calls this function with the current
 * function name as an argument. A Lua `number` value is considered an 'address' if it does not
 * exceed #LUA_FLOAT_ADDRESS_MAX.
 *
 * @param L The Lua state.
 * @param idx An index into the Lua stack.
 * @param function The name of the current Lua autosplitter function. Used in the error message. If
 * `NULL`, don't print anything.
 * @return True if the element in the Lua stack at `idx` represents a LuaJIT 64-bit integer, or a
 * floating-point number small enough to be converted into one.
 */
bool lua_isaddress_print(lua_State* L, int idx, const char* function)
{
    if (lua_isint64(L, idx))
        return true;
    if (!lua_isnumber(L, idx)) {
        if (function != NULL)
            printf("[%s] ERROR: Argument must be numeric. Check your autosplitter code.\n", function);
        return false;
    }
    double addr = lua_tonumber(L, idx);
    uintptr_t addr_abs = fabs(addr);
    bool is_valid = fabs(addr) <= LUA_FLOAT_ADDRESS_MAX;
    if (!is_valid && function != NULL)
        printf("[%s] ERROR: Floating-point argument is too large to represent a valid integer. "
               "Please write %s0x%lxLL instead of %s0x%lx. Check your autosplitter code.\n",
            function,
            addr < 0 ? "-" : "", addr_abs,
            addr < 0 ? "-" : "", addr_abs);
    return is_valid;
}

/**
 * @brief Get a value from the Lua for use as an address or file offset.
 *
 * If the value is a negative signed value, we cast it into an unsigned integer. This is to
 * facilitate passing negative offsets as part of pointer paths in readAddress(), which may be
 * useful for dealing with some structs.
 *
 * @param L The Lua state.
 * @param idx An index into the Lua stack.
 * @return The value at `idx` as an unsigned integer, or 0 if it is not usable as an address as
 * determined by lua_isaddress().
 */
uintptr_t lua_toaddress(lua_State* L, int idx)
{
    if (!lua_isaddress(L, idx))
        return 0;
    if (lua_isint64(L, idx))
        return lua_toint64(L, idx);
    uintptr_t addr = lua_tonumber(L, idx);
    return addr;
}

/**
 * @brief Alternative implementation of Lua's concatenation (`..`) operator; see
 * setup_int64_overloads() for more details.
 *
 * Takes two arguments `x` and `y` of type `string | number | int64` from the Lua stack and returns
 * a single string representing `x .. y`.
 *
 * @param L The Lua state
 */
static int int64_concat_metamethod(lua_State* L)
{
    assert(lua_gettop(L) == 2);
    for (int i = 1; i <= 2; i++) {
        if (!lua_isnumber(L, i) && !lua_isstring(L, i) && !lua_isint64(L, i)) {
            char* error;
            int status = asprintf(&error, "attempt to concatenate '%s' and '%s'", lua_typename(L, 1), lua_typename(L, 2));
            if (status == -1) {
                perror("Failed to generate error message");
                exit(EXIT_FAILURE);
            }
            lua_pushstring(L, error);
            free(error);
            lua_error(L);
        }
    }
    stringify(L, 1);
    stringify(L, 2);
    lua_concat(L, 2);
    assert(lua_gettop(L) == 1);
    return 1;
}

/**
 * @brief Integrate FFI integers with Lua operators such as string concatenation.
 *
 * Lua actually allows you to set metatables on primitive types like `number`, `string` and
 * `cdata`, but only from the C API. (See [§2.8 of the Lua manual](https://www.lua.org/manual/5.1/manual.html#2.8).)
 * The metatable applies to _all_ instances of the type, so metamethods can be used to change the
 * implementation for operators like `..` or `tostring()`. This method sets a metatable on `cdata`
 * values, overriding the `__concat` metamethod to support concatenation with FFI integers.
 *
 * @param L The Lua state.
 */
void setup_int64_overloads(lua_State* L)
{
    int initial_top = lua_gettop(L);
    lua_pushsint64(L, 0);
    if (!lua_getmetatable(L, -1))
        lua_createtable(L, 0, 1);
    lua_pushcfunction(L, int64_concat_metamethod);
    lua_setfield(L, -2, "__concat");
    lua_setmetatable(L, -2); // this sets the metatable on _all_ values of type `cdata`, not just the target one.
    lua_pop(L, 1);
    assert(lua_gettop(L) == initial_top);
}
