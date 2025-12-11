#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <linux/limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <luajit.h>

struct game_process {
    const char* name;
    int pid;
    uintptr_t base_address;
    uintptr_t dll_address;
};
typedef struct game_process game_process;

typedef struct ProcessMap {
    uintptr_t start;
    uint64_t size;
    char name[PATH_MAX];
} ProcessMap;
extern uint32_t p_maps_cache_size;

uintptr_t find_base_address(const char* module);
int process_exists();
int find_process_id(lua_State* L);
int getPid(lua_State* L);
bool parseMapsLine(const char* line, ProcessMap* map);
int lua_get_module_size(lua_State* L);

#endif /* __PROCESS_H__ */
