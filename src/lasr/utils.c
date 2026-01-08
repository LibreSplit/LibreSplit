#include "utils.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

game_process process;
ProcessMap p_maps_cache[MAPS_CACHE_MAX_SIZE];
uint32_t p_maps_cache_size = 0;
int maps_cache_cycles_value = 1; /*!< The number of cycles the cache is active for */

bool parseMapsLine(const char* line, ProcessMap* map)
{
    uintptr_t end;
    uint64_t size;
    char mode[8];
    unsigned long offset;
    unsigned int major_id, minor_id, node_id;

    // Thank you kernel source code
    int sscanf_res = sscanf(line, "%lx-%lx %7s %lx %u:%u %u %s", &map->start,
        &end, mode, &offset, &major_id,
        &minor_id, &node_id, map->name);
    if (!sscanf_res)
        return false;

    // Calculate the map size
    size = end - map->start;
    map->size = size;
    return true;
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

    for (uint32_t i = 0; i < p_maps_cache_size; i++) {
        const char* name = p_maps_cache[i].name;
        if (strstr(name, module_to_grep) != NULL) {
            return p_maps_cache[i].start;
        }
    }

    char path[22]; // 22 is the maximum length the path can be (strlen("/proc/4294967296/maps"))

    snprintf(path, sizeof(path), "/proc/%d/maps", process.pid);

    FILE* f = fopen(path, "r");

    if (f) {
        char current_line[PATH_MAX + 100];
        while (fgets(current_line, sizeof(current_line), f) != NULL) {
            if (strstr(current_line, module_to_grep) == NULL)
                continue;
            fclose(f);
            uintptr_t addr_start = strtoull(current_line, NULL, 16);
            if (maps_cache_cycles_value != 0 && p_maps_cache_size < MAPS_CACHE_MAX_SIZE) {
                ProcessMap map;
                if (parseMapsLine(current_line, &map)) {
                    p_maps_cache[p_maps_cache_size] = map;
                    p_maps_cache_size++;
                }
            }
            return addr_start;
        }
        fclose(f);
    }
    printf("Couldn't find base address\n");
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