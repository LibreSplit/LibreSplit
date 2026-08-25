#include "utils.h"
#include "../gui/dialogs.h"
#include "./auto-splitter.h"
#include "./maps/maps.h"

#include <glib.h>
#include <stdatomic.h>
#include <stdio.h>

extern borrowed_data * shared_globals;

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

// NOTE: key is not strduped right now
owned_data * register_shared_global(char const * key)
{
	// TODO: Reject this call if the autosplitter is running (very unsafe!!!!!)

	owned_data * new = malloc(sizeof(owned_data));
	if (!new) {
		return NULL;
	}

	new->key = key;
	new->next = (owned_data *) shared_globals;
	new->type = LASR_VOID; // nil aka "no value"
	atomic_store(&new->data.atomic, 0);
	atomic_store(&new->state, LASR_STATE_OWNED);

	/* Attach the new value to the front of the linked list */
	shared_globals = (borrowed_data *) new;

	return new;
}

/**
 * Retrieves a shared value exported by the timer into a buffer.
 * Buffer should always be 4(?) bytes wide?? See the union in borrow.h for now.
 * It may make more sense to typedef this as well.

 * Also, naming is hard. Import implies it's a one-time thing sorta, but I also
 * want to indicate that this call is the "other half" of the state machine and
 * therefore important.

 * TODO: Not fully sure if this is the best place for this implementation, but
 * this location follows suit with restart_auto_splitter being an operation
 * that lets the timer thread interact with the splitter thread.

 * *Also* TODO: This docu-string.
 * Returns the type that was stored.
 * If that return type is a LASR_STRING it must be freed!!
 */
int import_shared_global(owned_data* container, shared_data* buf)
{
	int value;
	int type = -1;
	size_t size = 0;

	if (!container) {
		return -1;
	}

	switch(atomic_load(&container->state)) {
		default:
			// sanity check (TODO: maybe remove me)
			printf("Unexpected state while loading: %d\n", atomic_load(&container->state));
			// fallthrough
		case LASR_STATE_NEEDED: // New data incompatible, no change (yet)
			atomic_store(&container->state, LASR_STATE_BORROWED);
			// fallthrough
		case LASR_STATE_BORROWED: // No change
			// TODO: Using -1 here doesn't really work with the enum typedef
			return -1;

		case LASR_STATE_ATOMIC: // Data is trustworthy
			value = atomic_load(&container->data.atomic);
			if (buf) {
				memcpy(buf, &value, sizeof(int));
			}
			type = (int) container->type;
			break;

		case LASR_STATE_OWNED:
			type = (int) container->type;
			
			if (type == LASR_STRING) {
				size = container->data.dynamic->size;
				
				// Treat buf as a char ** instead
				// Create a new string_data to copy to
				if (buf) {
					if ((buf->dynamic = malloc(size))) {
						memcpy(buf->dynamic, container->data.dynamic, size);
					}
					else {
						printf("not enough memory for string import\n");
						type = -1;
					}
				}
			}

			// Pass this back to the lua thread
			atomic_store(&container->state, LASR_STATE_BORROWED);
			break;
	}
	return type;
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
