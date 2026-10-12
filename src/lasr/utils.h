#pragma once

#include <linux/limits.h>
#include <lua.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

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
    char* name; /*!< A copy of the name of the process */
    unsigned int pid; /*!< The PID of the process */
    uintptr_t base_address; /*!< The detected base address of the process */
    uintptr_t dll_address; /*!< The detected base address of the last requested module */
} game_process;
extern game_process process;

typedef struct ProcessMap {
    uintptr_t start;
    uintptr_t end;
    uintptr_t size;
    char name[PATH_MAX + 1];
} ProcessMap;

typedef enum PointerSize {
    POINTER_SIZE_UNKNOWN = 0,
    POINTER_SIZE_32 = 32,
    POINTER_SIZE_64 = 64,
} PointerSize;

bool restart_auto_splitter(void);
uintptr_t find_base_address(const char* module);
bool handle_memory_error(uint32_t err);
const char* value_to_c_string(lua_State* L, int index);
PointerSize detect_pointer_size(uint64_t module_address, int32_t* err);
