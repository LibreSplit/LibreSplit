#include "process.h"

#include "../utils.h"

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
 * @param size The size of `output` including the terminating null byte.
 */
void execute_command(const char* const argv[], char* output, size_t size)
{
    pid_t pid;
    FILE* pipe = pvopen(argv, "r", &pid);
    if (!pipe) {
        fprintf(stderr, "Error executing command: { ");
        while (*argv != NULL)
            fprintf(stderr, "'%s', ", *argv++);
        fprintf(stderr, "}");
        exit(1);
    }

    size_t read = fread(output, 1, size - 1, pipe);
    output[read] = 0;
    char* newline = strrchr(output, '\n');
    if (newline != NULL)
        *(newline + 1) = 0;

    pvclose(pipe, pid);
}

void stock_process_id(const char* const argv[])
{
    char pid_output[PATH_MAX + 100];
    pid_output[0] = '\0';

    while (atomic_load(&auto_splitter_enabled)) {
        execute_command(argv, pid_output, sizeof(pid_output));
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

    if (strcmp(sort, "first") == 0)
        stock_process_id((const char*[]) { "pgrep", process.name, NULL });
    else
        stock_process_id((const char*[]) { "sh", "-c", "pgrep \"$1\" | sort --reverse --numeric-sort", "sh", process.name, NULL });

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

    if (strcmp(sort, "first") == 0)
        stock_process_id((const char*[]) { "pgrep", "--full", process.name, NULL });
    else
        // We pass --ignore-ancestors to stop pgrep from detecting itself
        stock_process_id((const char*[]) { "sh", "-c", "pgrep --ignore-ancestors --full \"$1\" | sort --reverse --numeric-sort", "sh", process.name, NULL });

    return 0;
}
