#include "getModuleSize.h"

#include "../utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint64_t get_module_size_from_map(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    // Check the cache
    for (uint32_t i = 0; i < p_maps_cache_size; i++) {
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
int getModuleSize(lua_State* L)
{
    if (lua_gettop(L) == 0) {
        // Called without argument
        uint64_t size = get_module_size_from_map(NULL);
        lua_pushinteger(L, (lua_Integer)size);
        return 1;
    }
    if (lua_isnil(L, 1)) {
        // Called with "nil" as argument
        uint64_t size = get_module_size_from_map(NULL);
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
    uint64_t size = get_module_size_from_map(module_name);
    lua_pushinteger(L, (lua_Integer)size);
    return 1;
}