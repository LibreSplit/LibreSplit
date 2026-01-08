#pragma once

#include <linux/limits.h>
#include <lua.h>
#include <stdint.h>

#include <sys/uio.h>
ssize_t process_vm_readv(pid_t pid,
    const struct iovec* local_iov,
    unsigned long liovcnt,
    const struct iovec* remote_iov,
    unsigned long riovcnt,
    unsigned long flags);

/**
 * \struct game_process The game process read by the Auto Splitter
 */
typedef struct game_process {
    const char* name; /*!< The name of the process */
    int pid; /*!< The PID of the process */
    uintptr_t base_address; /*!< The detected base address of the process */
    uintptr_t dll_address; /*!< The detected base address of the last requested module */
} game_process;
extern game_process process;

typedef struct ProcessMap {
    uintptr_t start;
    uintptr_t end;
    uintptr_t size;
    char name[PATH_MAX];
} ProcessMap;

#define MAPS_CACHE_MAX_SIZE 32
extern ProcessMap p_maps_cache[MAPS_CACHE_MAX_SIZE];
extern uint32_t p_maps_cache_size;
/**
 * Same as `maps_cache_cycles` but this one represents the current value
 * that changes on each cycle rather than the reference from the script
 */
extern int maps_cache_cycles_value;

bool parseMapsLine(const char* line, ProcessMap* map);
uintptr_t find_base_address(const char* module);
bool handle_memory_error(uint32_t err);
const char* value_to_c_string(lua_State* L, int index);
