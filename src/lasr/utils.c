#include "utils.h"
#include "../gui/dialogs.h"
#include "./auto-splitter.h"
#include "./maps/maps.h"

#include <glib.h>
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
 * Associates a 'lasr_global' container tracking a lua value with the lua
 * runtime itself. Omitting this call, the value will remain unchanged
 * regardless of the lua state.
 *
 * NOTE: This cannot be called while the splitter is running
 *
 * @param container A non-null reference to a 'lasr_global' container to be
 * tracked.
 */
void register_shared_global(lasr_global* new)
{
    if (!new || atomic_load(&auto_splitter_running)) {
        /* Reject this call if the autosplitter is running (developer error if
         * this happens.)
         * The linked list is not atomic, so we are certain to spontaneously
         * crash if this were ignored */
        printf("Reject registration of export var `%s`\n", new ? new->key : "<none>");
        return;
    }

    new->next = shared_globals;
    shared_globals = new;
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
