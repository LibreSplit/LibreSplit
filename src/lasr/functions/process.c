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
 * Compares string against ignore names
 *
 * @param n the name of the process to compare
 *
 * @return 1 for not in ignore names, 0 for found in ignore names
 */
int name_okay(const char* n)
{
    int i = 0;
    while (process.ignore_names[i] != 0 && strstr(n, process.ignore_names[i]) == 0)
        i++;
    return process.ignore_names[i] == 0;
}

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

    // procfs is special so the order should always be : "." ".." "a-Z" "PIDs 1..10..15..22..230"
    // skip '.', '..', and non-digit paths - NOP, formatter doesn't like { } below while
    while ((entry = readdir(dir)) != NULL && !isdigit(*entry->d_name)) { }

    entry = readdir(dir); // pid 1, init may be running stuff
    // gets rid of a strcmp() every loop :)

    while ((entry = readdir(dir)) != NULL) {
        char cmdl_path[PATH_MAX + 100];
        snprintf(cmdl_path, sizeof(cmdl_path), "/proc/%s/cmdline", entry->d_name);

        FILE* fp = fopen(cmdl_path, "r");
        if (fp) {
            char cmdl[2048] = { ' ' }; // only seen discord and chromium get bigger than this
            cmdl[2047] = 0x0;
            if (fgets(cmdl, 2048, fp) != NULL) {
                // turn the 0x0 spaces into ascii 0x20 spaces
                for (int i = 0; i < 2047; i++) {
                    if (cmdl[i] == 0) {
                        cmdl[i] = 0x20;
                    }
                }
                if (strstr(cmdl, proc_name) && name_okay(cmdl)) {
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

    // clear any old buffers before rebuilding ignore list
    // TODO: see run_auto_splitter, restarting a game rebuilds the table =/
    memset(process.mono_ignore_names, 0, 2048); // sizes in utils.h
    memset(process.ignore_names, 0, sizeof(char*) * 100);

    if (lua_istable(L, 3)) {
        size_t ignore_pos = 0; // ^inside mono array
        size_t ignore_cnt = 0; // hardcode of words in default_ignore_list
        size_t name_len = 0;
        ignore_cnt = 0;

        lua_pushnil(L); // bootstraps lua_next - see print_tbl.c
        while (lua_next(L, 3) != 0) {
            lua_pushvalue(L, -2); // key/index + value
            // __attribute__((unused)) const char* jnk = lua_tostring(L, -1); // key
            const char* ignore_name = lua_tostring(L, -2); // value, process name to ignore
            name_len = strlen(ignore_name);
            if (ignore_pos + name_len > 2047 || ignore_cnt > 98) {
                printf("[process] oversized ignore list! Currently hardcoded max size of 2048 for all names and 100 max names. Ignoring from %s on\n", ignore_name);
                lua_pop(L, 2);
                break;
            }
            memcpy(&process.mono_ignore_names[ignore_pos], ignore_name, name_len);
            process.ignore_names[ignore_cnt] = &process.mono_ignore_names[ignore_pos];
            ignore_pos += name_len + 1; // skip null byte - text1\0text
            ignore_cnt++;
            lua_pop(L, 2);
        }
    } else {
        // TODO: I'm sure "steam" also has some conflicts? (default list below)
        memcpy(process.mono_ignore_names, "wine\0start.exe", 14);
        process.ignore_names[0] = process.mono_ignore_names;
        process.ignore_names[1] = process.mono_ignore_names + 5; // sizeof("wine") + 1(null byte)
    }

    while (atomic_load(&auto_splitter_enabled)) {
        process.pid = find_process_by_name(process.name, process.sort_fl);
        pid_t last_pid = process.pid;

        // HACK: some games run from launchers and change names after launch
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
