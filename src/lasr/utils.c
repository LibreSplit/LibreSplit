#include "utils.h"
#include "../gui/dialogs.h"
#include "./auto-splitter.h"
#include "./int64.h"
#include "./maps/maps.h"

#include <glib.h>
#include <lua.h>

#include <stdatomic.h>
#include <stdio.h>

game_process process;

/**
 * Restarts the auto splitter by disabling it and re-enabling it again
 *
 * @return true if the auto splitter was enabled before the restart, false otherwise
 */
bool restart_auto_splitter(void)
{
    const bool was_asl_enabled = atomic_load(&auto_splitter_enabled);
    if (was_asl_enabled) {
        atomic_store(&auto_splitter_enabled, false);
        while (atomic_load(&auto_splitter_running) && was_asl_enabled) {
            // wait, this will be very fast so its ok to just spin
        }
        atomic_store(&auto_splitter_enabled, true);
    }
    return was_asl_enabled;
}

/**
 * Gets the base address of a module.
 *
 * @param module The module name for which to find the base address of. If NULL, the main process is used.
 *
 * @return The base address of the chosen module.
 */
uintptr_t find_base_address(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    ProcessMap map;
    const bool found = maps_findMapByName(module_to_grep, &map);
    if (found) {
        return map.start;
    }
    return 0;
}

/**
 * Prints a memory error to stdout.
 *
 * @param err The error code to print.
 *
 * @return True if the error was printed, false if the error is unknown.
 */
bool handle_memory_error(uint32_t err)
{
    static bool shownDialog = false;
    if (err == 0)
        return false;
    switch (err) {
        case EFAULT:
            printf("[readAddress] EFAULT: Invalid memory space/address\n");
            break;
        case EINVAL:
            printf("[readAddress] EINVAL: An error ocurred while reading memory\n");
            break;
        case ENOMEM:
            printf("[readAddress] ENOMEM: Please get more memory\n");
            break;
        case EPERM:
            printf("[readAddress] EPERM: Permission denied\n");

            if (!shownDialog) {
                shownDialog = true;
                g_idle_add(display_non_capable_mem_read_dialog, NULL);
            }

            break;
        case ESRCH:
            printf("[readAddress] ESRCH: No process with specified PID exists\n");
            break;
    }
    return true;
}

/**
 * @brief Convert a Lua value in-place to a string by calling `tostring()`.
 *
 * Like `lua_tostring()`, the value at `index` is replaced with a string value if it is not already
 * a string.
 *
 * @param L The Lua state.
 * @param index An index into the Lua stack.
 * @return A pointer to a string inside the Lua state.
 */
const char* stringify(lua_State* L, int index)
{
    int initial_top = lua_gettop(L);
    if (lua_isstring(L, index) || lua_isnumber(L, index))
        return lua_tostring(L, index);
    lua_getglobal(L, "tostring");
    int new_index = index < 0 ? index - 1 : index;
    lua_pushvalue(L, new_index);
    lua_call(L, 1, 1);
    lua_replace(L, new_index);
    assert(lua_gettop(L) == initial_top && lua_isstring(L, index));
    return lua_tostring(L, index);
}

/**
 * Utility function to convert a lua value to a string.
 *
 * Converts a value to a printable C string according to its type.
 * This is due to lua_tostring returning "null" for booleans and
 * other non-string types.
 */
const char* value_to_c_string(lua_State* L, int index)
{
    switch (lua_type(L, index)) {
        case LUA_TSTRING:
            return lua_tostring(L, index);
        case LUA_TNUMBER:
            return lua_tostring(L, index);
        case LUA_TCDATA:
            if (lua_isint64(L, index))
                return stringify(L, index);
            return "??";
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TNIL:
            return "nil";
        default:
            return "??";
    }
}
