#include "utils.h"
#include "src/lasr/maps/maps.h"

#include <glib.h>
#include <stdio.h>

game_process process;

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

gboolean display_non_capable_mem_read_dialog(void* data);

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
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TNIL:
            return "nil";
        default:
            return "??";
    }
}