#include "process.h"

#include "../auto-splitter.h"
#include "../utils.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern atomic_bool auto_splitter_enabled; /*!< Defines if the auto splitter is enabled */

static const char* normalize_sort(const char* sort)
{
    if (!sort) {
        return "first";
    }

    if (strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
        printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to first\n", sort);
        return "first";
    }

    return sort;
}

static void build_process_command(const process_query* query, char* command, size_t command_size)
{
    char sortCmd[16] = "";
    if (strcmp(query->sort, "last") == 0) {
        strcpy(sortCmd, " | sort -r");
    }
    if (query->kind == PROCESS_LOOKUP_CMDLINE) {
        snprintf(command, command_size, "pgrep -f \"%.*s\"%s",
            (int)strnlen(query->name, command_size - strlen(sortCmd) - 1),
            query->name, sortCmd);
    } else {
        snprintf(command, command_size, "pgrep \"%.*s\"%s",
            (int)strnlen(query->name, command_size - strlen(sortCmd) - 1),
            query->name, sortCmd);
    }
}

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

bool try_find_process(const process_query* query)
{
    char pid_output[PATH_MAX + 100];
    char command[256];
    pid_output[0] = '\0';
    build_process_command(query, command, sizeof(command));
    execute_command(command, pid_output);
    process.pid = strtoul(pid_output, NULL, 10);
    if (!process.pid) {
        return false;
    }
    size_t newlinePos = strcspn(pid_output, "\n");
    if (newlinePos != strlen(pid_output) - 1 && pid_output[0] != '\0') {
        printf("Multiple PID's found for process: %s\n", query->name);
    }
    process.name = process_lookup.name;
    printf("Process: %s\n", process.name);
    printf("PID: %u\n", process.pid);
    return true;
}

bool wait_for_process(const process_query* query, const char* current_file)
{
    while (!runtime_should_stop(current_file)) {
        if (try_find_process(query)) {
            return true;
        }
        printf("%s isn't running.\n", query->name);
        usleep(100000);
    }
    return false;
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
 *
 * @return Always zero.
 */
int find_process_id(lua_State* L)
{
    printf("\033[2J\033[1;1H"); // Clean the console

    process_lookup.kind = PROCESS_LOOKUP_NAME;
    strncpy(process_lookup.name, lua_tostring(L, 1), sizeof(process_lookup.name) - 1);

    process_lookup.name[sizeof(process_lookup.name) - 1] = '\0';
    strncpy(process_lookup.sort, normalize_sort(lua_tostring(L, 2)), sizeof(process_lookup.sort) - 1);

    process_lookup.sort[sizeof(process_lookup.sort) - 1] = '\0';
    process_lookup_configured = true;
    process.name = process_lookup.name;
    wait_for_process(&process_lookup, auto_splitter_file);
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
    printf("\033[2J\033[1;1H");
    process_lookup.kind = PROCESS_LOOKUP_CMDLINE;
    strncpy(process_lookup.name, lua_tostring(L, 1), sizeof(process_lookup.name) - 1);
    process_lookup.name[sizeof(process_lookup.name) - 1] = '\0';
    strncpy(process_lookup.sort, normalize_sort(lua_tostring(L, 2)), sizeof(process_lookup.sort) - 1);
    process_lookup.sort[sizeof(process_lookup.sort) - 1] = '\0';
    process_lookup_configured = true;
    process.name = process_lookup.name;
    wait_for_process(&process_lookup, auto_splitter_file);
    return 0;
}
