#pragma once

#include <linux/limits.h>
#include <lua.h>

#include <stdbool.h>
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
    unsigned int pid; /*!< The PID of the process */
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

typedef enum {
    PROCESS_LOOKUP_NAME,
    PROCESS_LOOKUP_CMDLINE
} process_lookup_kind;

typedef struct process_query {
    process_lookup_kind kind;
    char name[PATH_MAX];
    char sort[16];
} process_query;

extern process_query process_lookup;
extern bool process_lookup_configured;

bool restart_auto_splitter(void);
uintptr_t find_base_address(const char* module);
bool handle_memory_error(uint32_t err);
const char* value_to_c_string(lua_State* L, int index);
bool try_find_process(const process_query* query);
bool wait_for_process(const process_query* query, const char* current_file);
bool runtime_should_stop(const char* current_file);
bool has_lua_function(lua_State* L, const char* name);
bool process_is_attached(void);
