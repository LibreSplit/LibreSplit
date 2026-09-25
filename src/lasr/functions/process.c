#include "process.h"

#include "../utils.h"
#include "src/lasr/maps/maps.h"
#include "src/logging.h"

#include <ctype.h>
#include <dirent.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern atomic_bool auto_splitter_enabled; /*!< Defines if the auto splitter is enabled */

/**
 * Reads the /proc/ directory for all process IDs and tries to identify the searched
 * process by name, depending on the mode.
 *
 * @param[in] mode Can only be "comm" or "cmdline", searches for the /proc/pid/comm or
 *             /proc/pid/cmdline for a match.
 * @param[in] name The name of the process to look for
 * @param[out] output A pointer to an array, to fill with possible PIDs.
 * @param[out] output_count A pointer to a counter, to determine how many PIDs were found.
 *
 * @returns Zero if all goes well, an error code otherwise
 */
static int get_all_pids_by_name(const char* mode, const char* name, pid_t** output, size_t* output_count)
{
    if (strcmp(mode, "comm") != 0 && strcmp(mode, "cmdline") != 0) {
        LOG_ERRF("Search mode %s not supported", mode);
        return -1;
    }
    *output = NULL;
    *output_count = 0;

    DIR* proc = opendir("/proc");
    if (!proc) {
        LOG_ERR("Cannot open /proc directory");
        return -1;
    }

    pid_t* pids = NULL;
    size_t count = 0;

    struct dirent* entry;

    while ((entry = readdir(proc)) != NULL) {
        const char* dirname = entry->d_name;

        if (*dirname == '\0') {
            // Dirname is just the null terminator, skip
            continue;
        }

        while (*dirname) {
            if (!isdigit((unsigned char)*dirname)) {
                // We found a non-numeric char in the directory name, that's enough
                break;
            }
            dirname++;
        }

        if (*dirname != '\0') {
            // Dirname didn't reach the null terminator -> non-numeric name -> skip
            continue;
        }

        pid_t pid = (pid_t)strtoul(entry->d_name, NULL, 10);

        char path[256];
        snprintf(path, sizeof(path), "/proc/%u/%s", pid, mode);

        FILE* file = fopen(path, "r");
        if (!file) {
            // The process may have exited during the scan, skip
            continue;
        }

        char comm[256];

        if (fgets(comm, sizeof(comm), file)) {
            // Replace \n with a NUL
            comm[strcspn(comm, "\n")] = '\0';

            // We use a "contains" check to accept the new PID
            if (strstr(comm, name) != NULL) {
                pid_t* new_pids = realloc(pids, (count + 1) * sizeof(*output));
                if (!new_pids) {
                    // Malloc fail, bail out
                    free(pids);
                    fclose(file);
                    closedir(proc);
                    return -1;
                }

                // Malloc ok, switchover time
                pids = new_pids;
                pids[count++] = pid;
            }
        }
        fclose(file);
    }
    closedir(proc);
    *output = pids;
    *output_count = count;

    return 0;
}

/**
 * Comparison function for the "first" sorting method, used in qsort
 *
 * @param a The first comparison operator
 * @param b The second comparison operator
 *
 * @returns -1,0,1 according to the relative ordering of a and b
 */
static int compare_pids_ascending(const void* a, const void* b)
{
    const pid_t pid_a = *(const pid_t*)a;
    const pid_t pid_b = *(const pid_t*)b;
    if (pid_a < pid_b) {
        return -1;
    }
    if (pid_a > pid_b) {
        return 1;
    }
    return 0;
}

/**
 * Comparison function for the "last" sorting method, used in qsort
 *
 * @param a The first comparison operator
 * @param b The second comparison operator
 *
 * @returns -1,0,1 according to the relative ordering of a and b
 */
static int compare_pids_descending(const void* a, const void* b)
{
    const pid_t pid_a = *(const pid_t*)a;
    const pid_t pid_b = *(const pid_t*)b;
    if (pid_a < pid_b) {
        return -1;
    }
    if (pid_a > pid_b) {
        return 1;
    }
    return 0;
}

/**
 * Searches for the pid of a certain process.
 *
 * @param mode The search mode, can be only "comm" (limited to 15 char) and
 *             "cmdline" (the full command)
 * @param sort The sorting method used to extract the PID, can only be "first"
 *             or "last"
 * @param name The name of the process to look for
 *
 * @returns The PID of the process that is searched for (zero if no process is found)
 */
static pid_t get_pid(const char* mode, const char* sort, const char* name)
{
    if (strcmp(mode, "comm") != 0 && strcmp(mode, "cmdline") != 0) {
        LOG_ERRF("Search mode %s not supported", mode);
        return 0;
    }
    if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
        LOG_ERRF("Search mode %s not supported", mode);
        printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
        sort = "first";
    }

    pid_t* pids = NULL;
    size_t count = 0;
    get_all_pids_by_name(mode, name, &pids, &count);

    if (!pids) {
        return 0;
    }

    if (strcmp(sort, "first") == 0) {
        qsort(pids, count, sizeof(pids[0]), compare_pids_ascending);
    }
    if (strcmp(sort, "last") == 0) {
        qsort(pids, count, sizeof(pids[0]), compare_pids_descending);
    }

    pid_t result = pids[0];
    free(pids);
    pids = NULL;
    count = 0;
    return result;
}

void stock_process_id(const char* mode, const char* sort, const char* name)
{
    // We just started a new process monitoring, we may want to clean up a stale cache
    maps_clearCache();

    while (atomic_load(&auto_splitter_enabled)) {
        process.pid = get_pid(mode, sort, name);
        if (!process.pid) {
            printf("%s isn't running.\n", process.name);
            usleep(100000); // Sleep for 100ms
        } else {
            break;
        }
    }

    printf("Process: %s\n", process.name);
    printf("PID: %u\n", process.pid);
    process.base_address = find_base_address(NULL);
    process.dll_address = process.base_address;
}

/**
 * Finds the ID of the process indicated by the Lua Auto Splitter.
 *
 * @param L The Lua State.
 *
 * @return Always zero.
 */
int find_process_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clear the console

    process.name = lua_tostring(L, 1);
    const char* sort = lua_tostring(L, 2);

    if (!sort) {
        sort = "first";
    } else {
        if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
            printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
            sort = "first";
        }
    }

    stock_process_id("comm", sort, process.name);

    return 0;
}

/**
 * Finds the ID of the process indicated by the Lua Auto Splitter using full commandline grepping.
 *
 *  NOTE: [Penaz] [2026-04-25] This differs from find_process_id only by the -f argument. Consider
 *  ^ merging the command creation into a single function instead of duplicating code.
 *
 * @param L The Lua State.
 *
 * @return Always zero.
 */
int find_cmdline_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clear the console

    process.name = lua_tostring(L, 1);
    const char* sort = lua_tostring(L, 2);

    if (!sort) {
        sort = "first";
    } else {
        if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
            printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
            sort = "first";
        }
    }

    stock_process_id("cmdline", sort, process.name);

    return 0;
}
