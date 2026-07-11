#include "process.h"

#include "../utils.h"

#include <ctype.h>
#include <dirent.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern atomic_bool auto_splitter_enabled; /*!< Defines if the auto splitter is enabled */

/**
 * Finds a process by crawling /proc for proc_name
 *
 * @param proc_name the name of the process
 *
 * @param fl to grab the first or last process
 *
 * @return PID or 0 for error or not found
 */
pid_t find_process_by_name(const char* proc_name, int fl)
{
    DIR* dir;
    struct dirent* entry;
    pid_t pid = 0;

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc failed");
        return pid;
    }

    // skip '.', '..', and non-digit paths, these should always come before PIDs, procfs is special
    while ((entry = readdir(dir)) != NULL && !isdigit(*entry->d_name)) { }
    // NOP, formatter doesn't like readability of { } down here

    entry = readdir(dir); // pid 1, init may be running stuff
    // gets rid of a strcmp() every loop :)

    while ((entry = readdir(dir)) != NULL) {
        int match = 0; // used more for inside the loop to skip checking comm AND cmdline
        char comm_path[PATH_MAX + 100];
        char cmdl_path[PATH_MAX + 100];

        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        FILE* fp = fopen(comm_path, "r");
        if (fp) {
            char comm[512] = { 0 };
            if (fgets(comm, sizeof(comm), fp) != NULL) {
                // ignore wine starting processes (see HACK below)
                if (strstr(comm, proc_name) != 0
                    && strstr(comm, "wine") == 0
                    && strstr(comm, "start.exe") == 0) {
                    match = 1;
                    pid = strtoul(entry->d_name, NULL, 10);
                    if (fl == 0) {
                        fclose(fp);
                        break; // first hit, otherwise keep going for last
                    }
                }
            }
            fclose(fp);
        }

        if (match) {
            continue; // skip lengthy cmdline check if comm matches
        }

        snprintf(cmdl_path, sizeof(cmdl_path), "/proc/%s/cmdline", entry->d_name);
        fp = fopen(cmdl_path, "r");
        if (fp) {
            char cmdl[2048] = { ' ' }; // can be much bigger than comm
            cmdl[2047] = 0x0;
            if (fgets(cmdl, 2048, fp) != NULL) {
                // turn the 0x0 spaces into ascii 0x20 spaces
                for (int i = 0; i < 2047; i++) {
                    if (cmdl[i] == 0) {
                        cmdl[i] = 0x20;
                    }
                }
                // ignore wine starting processes (see HACK below)
                if (strstr(cmdl, proc_name) != 0
                    && strstr(cmdl, "wine") == 0
                    && strstr(cmdl, "start.exe") == 0) {
                    pid = strtoul(entry->d_name, NULL, 10);
                    if (fl == 0) {
                        fclose(fp);
                        break; // first hit, otherwise keep going for last
                    }
                }
            }
            fclose(fp);
        }
    }

    closedir(dir);
    return pid;
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

    const char* tmp_name = lua_tostring(L, 1);
    const char* sort = lua_tostring(L, 2);
    // if you're not first, you're last ('last' is the only valid argument)
    int is_first = sort ? strcmp(sort, "first") : -1;
    int is_last = sort ? strcmp(sort, "last") : -1;
    process.sort_fl = is_last == 0 ? 1 : 0;

    if (process.name) {
        free(process.name); // clear in case something is using it
        process.name = 0; // the auto-splitter loop checks for name
    }
    // the lua stack will constantly realloc and the process name above WILL get corrupted
    process.name = (char*)calloc(strlen(tmp_name) + 1, sizeof(char));
    memcpy(process.name, tmp_name, strlen(tmp_name));

    if (sort && is_first != 0 && is_last != 0) {
        printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
    }

    while (atomic_load(&auto_splitter_enabled)) {
        process.pid = find_process_by_name(process.name, process.sort_fl);
        pid_t last_pid = process.pid;

        // HACK: wine can run a 'start.exe' command to launch a game
        // at that time, the game's process name is in wine's cmdline args
        // also, some games launch and then change names, or launch the game proper
        // that's not the game :)
        // may also be good enough to check the base/dll addys ? 0 indicates wine?
        usleep(200000); // Sleep for 200ms ? may be game/hardware dependant
        process.pid = find_process_by_name(process.name, process.sort_fl);
        if (process.pid && process.pid == last_pid) {
            break;
        }
        printf("%s isn't running.\n", process.name);
        usleep(100000); // Sleep for 100ms
    }

    printf("Process: %s\n", process.name);
    printf("PID: %u\n", process.pid);
    process.base_address = find_base_address(NULL);
    process.dll_address = process.base_address;

    return 0;
}
