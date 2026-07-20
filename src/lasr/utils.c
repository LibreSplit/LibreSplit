#include "utils.h"
#include "../gui/dialogs.h"
#include "./auto-splitter.h"
#include "./maps/maps.h"

#include <glib.h>

#include <assert.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/wait.h>

extern char** environ;

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

/**
 * @brief Like `popen()`, but you can specify the argv.
 *
 * Searches for `argv[0]` in the current `PATH` and executes it with an argument list of `argv`.
 *
 * @param argv The NULL-terminated argument list for the program.
 * @param mode `"r"` or `"w"` depending on whether you want to read from or write to the child process's standard IO.
 * @param pid[out] The spawned process's ID.
 * @copyright Adapted from the musl implementation of `popen()`.
 * @return A `FILE` stream linked to the process's standard input or output, or NULL on error.
 */
FILE* pvopen(const char* const argv[], const char* mode, pid_t* pid)
{
    int pipefd[2];
    posix_spawn_file_actions_t actions = { 0 };

    assert(*mode == 'r' || *mode == 'w');
    assert(!strchr(mode, 'e') && "FD_CLOEXEC mode not supported");
    int op = *mode == 'w';

    if (pipe(pipefd) == -1)
        return NULL;
    FILE* fp = fdopen(pipefd[op], mode);
    if (!fp) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    int err = 0;
    int backup_pid;
    if ((err = posix_spawn_file_actions_init(&actions)) != 0)
        goto fail_init;
    if ((err = posix_spawn_file_actions_adddup2(&actions, pipefd[1 - op], 1 - op)) != 0)
        goto fail;
    if ((err = posix_spawnp(pid ? pid : &backup_pid, argv[0], &actions, NULL, (char* const*)argv, environ)) != 0)
        goto fail;
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1 - op]);

    return fp;

fail:
    posix_spawn_file_actions_destroy(&actions);
fail_init:
    fclose(fp);
    close(pipefd[1 - op]);
    return NULL;
}

/**
 * @brief Close the IO stream produced by pvopen().
 * @param fp The IO stream returned by pvopen().
 * @param pid The process id returned by pvopen().
 * @copyright Adapted from the musl implementation of `pclose()`.
 * @return The exit status of the command.
 */
int pvclose(FILE* fp, pid_t pid)
{
    int status;
    fclose(fp);
    waitpid(pid, &status, 0);
    return status;
}
