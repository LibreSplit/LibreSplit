#include <linux/limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <luajit.h>

#include "auto-splitter.h"
#include "lua.h"
#include "process.h"

struct game_process process;
#define MAPS_CACHE_MAX_SIZE 32
ProcessMap p_maps_cache[MAPS_CACHE_MAX_SIZE];
uint32_t p_maps_cache_size = 0;

void execute_command(const char* command, char* output)
{
    char buffer[4096];
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "Error executing command: %s\n", command);
        exit(1);
    }

    while (fgets(buffer, 128, pipe) != NULL) {
        strcat(output, buffer);
    }

    pclose(pipe);
}

/*
Gets the base address of a module
if `module` equals to a nullptr, the main process is used, else it will search for the base addr of the specified module
*/
uintptr_t find_base_address(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    for (int32_t i = 0; i < p_maps_cache_size; i++) {
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

uint64_t get_module_size(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    // Check the cache
    for (int32_t i = 0; i < p_maps_cache_size; i++) {
        const char* name = p_maps_cache[i].name;
        if (strstr(name, module_to_grep) != NULL) {
            return p_maps_cache[i].size;
        }
    }

    // If not in cache, read the maps
    char path[22]; // 22 is the maximum length the path can be (strlen("/proc/4294967296/maps"))

    snprintf(path, sizeof(path), "/proc/%d/maps", process.pid);

    FILE* f = fopen(path, "r");

    if (f) {
        char current_line[PATH_MAX + 100];
        while (fgets(current_line, sizeof(current_line), f) != NULL) {
            if (strstr(current_line, module_to_grep) == NULL)
                continue;
            fclose(f);
            ProcessMap map;
            bool parsed = parseMapsLine(current_line, &map);
            if (maps_cache_cycles_value != 0 && p_maps_cache_size < MAPS_CACHE_MAX_SIZE) {
                if (parsed) {
                    p_maps_cache[p_maps_cache_size] = map;
                    p_maps_cache_size++;
                }
            }
            return map.size;
        }
        fclose(f);
    }
    printf("Couldn't find the module\n");
    return 0;
}

/**
 * The lua getModuleSize() function.
 *
 * FIXME: Size is taken as an unsigned long, which may
 *    ^   exceed the size of lua_Integer
 *
 * @param L The lua state
 */
int lua_get_module_size(lua_State* L)
{
    if (lua_gettop(L) == 0) {
        // Called without argument
        uint64_t size = get_module_size(NULL);
        lua_pushinteger(L, (lua_Integer)size);
        return 1;
    }
    if (lua_isnil(L, 1)) {
        // Called with "nil" as argument
        uint64_t size = get_module_size(NULL);
        lua_pushinteger(L, (lua_Integer)size);
        return 1;
    }
    if (!lua_isstring(L, 1)) {
        // Called with invalid non-string parameter
        printf("[getModuleSize] Module name must be a string or nil (for the main module)");
        lua_pushnil(L);
        return 1;
    }
    // Called with a module name (string)
    const char* module_name = lua_tostring(L, 1);
    uint64_t size = get_module_size(module_name);
    lua_pushinteger(L, (lua_Integer)size);
    return 1;
}

void stock_process_id(const char* pid_command)
{
    char pid_output[PATH_MAX + 100];
    pid_output[0] = '\0';

    while (atomic_load(&auto_splitter_enabled)) {
        execute_command(pid_command, pid_output);
        process.pid = strtoul(pid_output, NULL, 10);
        if (process.pid) {
            size_t newlinePos = strcspn(pid_output, "\n");
            if (newlinePos != strlen(pid_output) - 1 && pid_output[0] != '\0') {
                printf("Multiple PID's found for process: %s\n", process.name);
            }
            break;
        } else {
            printf("%s isn't running.\n", process.name);
            usleep(100000); // Sleep for 100ms
        }
    }

    printf("Process: %s\n", process.name);
    printf("PID: %u\n", process.pid);
    process.base_address = find_base_address(NULL);
    process.dll_address = process.base_address;
}

int find_process_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clear the console

    process.name = lua_tostring(L, 1);
    const char* sort = lua_tostring(L, 2);
    char sortCmd[16] = "";

    if (!sort) {
        sort = "first";
    } else {
        if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
            printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
            sort = "first";
        }
    }

    if (strcmp(sort, "first") == 0) {
        sortCmd[0] = '\0'; // No sorting
    }
    if (strcmp(sort, "last") == 0) {
        strcpy(sortCmd, " | sort -r"); // Reverse the sorting to get latest PID
    }

    char command[256];
    snprintf(command, sizeof(command), "pgrep \"%.*s\"%s", (int)strnlen(process.name, 15), process.name, sortCmd);

    stock_process_id(command);

    return 0;
}

int getPid(lua_State* L)
{
    lua_pushinteger(L, process.pid);
    return 1;
}

int process_exists()
{
    int result = kill(process.pid, 0);
    return result == 0;
}

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
