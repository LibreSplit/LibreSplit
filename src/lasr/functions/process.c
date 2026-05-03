#include "process.h"

#include "../utils.h"
#include "lua.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern atomic_bool auto_splitter_enabled; /*!< Defines if the auto splitter is enabled */

/**
 * Executes a command, piping its output into an output string.
 *
 * @param command The command to execute.
 * @param output Pointer to a string that will contain the command output.
 */
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

/**
 * Finds the ID of the process indicated by the Lua Auto Splitter.
 *
 * @param L The Lua State.
 * @param 1 The name of process to find.
 * @param 2 (optional) Force using full commandline grepping, pgrep -f.
 * @param 3 (optional) Sorting PID.
 *
 * Note: [AL] [2026-5-3] The sorting isn't documented or explained the use case. I'll leave it as it was before.
 *
 * @return Always zero.
 */
int find_process_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clear the console

    process.name = lua_tostring(L, 1);

    const char* cmdLineFlag = NULL;

    if (lua_isstring(L, 2)) {
        cmdLineFlag = lua_tostring(L, 2);
    }
    int forceUseCmdLine = cmdLineFlag && strcmp(cmdLineFlag, "cmdline") == 0;

    const char* sort = lua_tostring(L, 3);
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
    } else if (strcmp(sort, "last") == 0) {
        strcpy(sortCmd, " | sort -r"); // Reverse the sorting to get latest PID
    }

    char command[256];
    if (forceUseCmdLine || strlen(process.name) > 15) {
        snprintf(command, sizeof(command), "pgrep -f \"%.*s\"%s", (int)strnlen(process.name, sizeof(command) - strlen(sortCmd) - 1), process.name, sortCmd);
    } else {
        snprintf(command, sizeof(command), "pgrep \"%.*s\"%s", (int)strnlen(process.name, sizeof(command) - strlen(sortCmd) - 1), process.name, sortCmd);
    }
    stock_process_id(command);

    return 0;
}

/**
 * Checks the ID of the process indicated by the Lua Auto Splitter is running without attaching to it.
 * Used for making conditional statement when there is multiple game versions with different process ID.
 *
 * @param L The Lua State.
 * @param 1 The name of process to find.
 * @param 2 (optional) Force using full commandline grepping, pgrep -f.
 * @param 3 (optional) Sorting PID.
 *
 * @return is 1, returning a boolean value. True if process ID was found and false the otherwise.
 */
int check_process_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clear the console
                                //
    const char* name = lua_tostring(L, 1);

    const char* cmdLineFlag = NULL;

    if (lua_isstring(L, 2)) {
        cmdLineFlag = lua_tostring(L, 2);
    }
    int forceUseCmdLine = cmdLineFlag && strcmp(cmdLineFlag, "cmdline") == 0;

    const char* sort = lua_tostring(L, 3);
    char sortCmd[16] = "";

    char pid_output[PATH_MAX + 100];
    pid_output[0] = '\0';

    if (!sort) {
        sort = "first";
    } else {
        if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
            printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
            sort = "first";
        }
    }

    if (strcmp(sort, "first") == 0) {
        sortCmd[0] = '\0';
    } else if (strcmp(sort, "last") == 0) {
        strcpy(sortCmd, " | sort -r");
    }

    char command[256];
    if (forceUseCmdLine || strlen(name) > 15) {
        snprintf(command, sizeof(command), "pgrep -f \"%.*s\"%s", (int)strnlen(name, sizeof(command) - strlen(sortCmd) - 1), name, sortCmd);
    } else {
        snprintf(command, sizeof(command), "pgrep \"%.*s\"%s", (int)strnlen(name, sizeof(command) - strlen(sortCmd) - 1), name, sortCmd);
    }

    if (atomic_load(&auto_splitter_enabled)) {
        execute_command(command, pid_output);
    }
    unsigned long pid = strtoul(pid_output, NULL, 10);

    lua_pushboolean(L, pid != 0);
    return 1;
}
