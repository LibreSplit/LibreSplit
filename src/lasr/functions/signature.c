#include "signature.h"

#include "../utils.h"

#include <fcntl.h>
#include <inttypes.h>
#include <lua.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Error handling macro
#define HANDLE_ERROR(msg) \
    do {                  \
        perror(msg);      \
        return NULL;      \
    } while (0)

/**
 * Error logging function
 *
 * @param[out] format The format string
 */
void log_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error in sig_scan: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/**
 * Gets all the memory regions of a certain PID
 *
 * @param[in] pid The ID of the process to get the memory regions of
 * @param[in] count A pointer to a counter onto where to store the number of regions
 *
 * @return A dinamically allocated array of ProcessMap that have been found
 */
ProcessMap* get_memory_regions(pid_t pid, int* count)
{
    char maps_path[256];
    if (snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid) < 0) {
        HANDLE_ERROR("Failed to create maps path");
    }

    FILE* maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        HANDLE_ERROR("Failed to open maps file");
    }

    ProcessMap* regions = NULL;
    int capacity = 0;
    *count = 0;

    char line[256];
    while (fgets(line, sizeof(line), maps_file)) {
        if (*count >= capacity) {
            capacity = capacity == 0 ? 10 : capacity * 2;
            ProcessMap* temp = realloc(regions, capacity * sizeof(ProcessMap));
            if (!temp) {
                free(regions);
                fclose(maps_file);
                HANDLE_ERROR("Failed to allocate memory for regions");
            }
            regions = temp;
        }

        uintptr_t start, end;
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR, &start, &end) != 2) {
            continue; // Skip lines that don't match the expected format
        }
        regions[*count].start = start;
        regions[*count].end = end;
        (*count)++;
    }

    fclose(maps_file);
    return regions;
}

/**
 * Matches a pattern with an array of bytes.
 *
 * @param[in] data The data to compare the pattern against.
 * @param[in] pattern The pattern to test for.
 * @param[in] pattern_size The length of the pattern.
 *
 * @return True if the pattern matches the data, false otherwise
 */
bool match_pattern(const uint8_t* data, const uint16_t* pattern, size_t pattern_size)
{
    for (size_t i = 0; i < pattern_size; ++i) {
        uint8_t byte = pattern[i] & 0xFF;
        bool ignore = (pattern[i] >> 8) & 0x1;
        if (!ignore && data[i] != byte) {
            return false;
        }
    }
    return true;
}

/**
 * Converts an IDA-like signature into a pattern to be used in LibreSplit.
 * Supports the '??' string to ignore certain bytes in the comparison.
 *
 * @param[in] signature A string containing the signature to convert.
 * @param[out] pattern_size A pointer onto where to save the size of the pattern.
 *
 * @return A pattern to be used with the LibreSplit signature scan functions.
 */
uint16_t* convert_signature(const char* signature, size_t* pattern_size)
{
    char* signature_copy = strdup(signature);
    if (!signature_copy) {
        return NULL;
    }

    char* token = strtok(signature_copy, " ");
    size_t size = 0;
    size_t capacity = 10;
    uint16_t* pattern = (uint16_t*)malloc(capacity * sizeof(uint16_t));
    if (!pattern) {
        free(signature_copy);
        return NULL;
    }

    while (token != NULL) {
        if (size >= capacity) {
            capacity *= 2;
            uint16_t* temp = (uint16_t*)realloc(pattern, capacity * sizeof(uint16_t));
            if (!temp) {
                free(pattern);
                free(signature_copy);
                return NULL;
            }
            pattern = temp;
        }

        if (strstr(token, "?") != NULL) {
            // Treats a half-byte mask as a full-byte mask (0? => ?? or ?F=> ??)
            pattern[size] = 0xFF00; // Set the upper byte to 1 to indicate ignoring this byte
        } else {
            pattern[size] = strtol(token, NULL, 16);
        }
        size++;
        token = strtok(NULL, " ");
    }

    free(signature_copy);
    *pattern_size = size;
    return pattern;
}

bool validate_process_memory(pid_t pid, uintptr_t address, void* buffer, size_t size)
{
    struct iovec local_iov = { buffer, size };
    struct iovec remote_iov = { (void*)address, size };
    ssize_t nread = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);

    return nread == (ssize_t)size;
}

/**
 * Performs the Lua Auto Splitter sig_scan function, pushing onto the Lua stack the result.
 *
 * If a pattern is found, it will be offset by the process base_address, allowing the result to
 * be used directly in readAddress, without any module definition.
 *
 * Using readAddress with a module name and an address coming from sig_scan is not supported and
 * may result in out-of-process reads or other unforeseen consequences.
 *
 * @param L The lua state.
 *
 * @return Always 1 (one parameter is always pushed on the stack, either the address or nil)
 */
int perform_sig_scan(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        log_error("Invalid number of arguments: expected 2 (signature, offset)");
        lua_pushnil(L);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        log_error("Invalid argument types: expected (string, number)");
        lua_pushnil(L);
        return 1;
    }

    pid_t p_pid = process.pid;
    const char* signature = lua_tostring(L, 1);
    intptr_t offset = lua_tointeger(L, 2);

    // Validate signature string
    if (strlen(signature) == 0) {
        log_error("Signature string cannot be empty");
        lua_pushnil(L);
        return 1;
    }

    size_t pattern_length;
    uint16_t* pattern = convert_signature(signature, &pattern_length);
    if (!pattern) {
        log_error("Failed to convert signature");
        lua_pushnil(L);
        return 1;
    }

    int regions_count = 0;
    ProcessMap* regions = get_memory_regions(p_pid, &regions_count);
    if (!regions) {
        free(pattern);
        log_error("Failed to get memory regions");
        lua_pushnil(L);
        return 1;
    }

    for (int i = 0; i < regions_count; i++) {
        ProcessMap region = regions[i];
        ssize_t region_size = region.end - region.start;
        uint8_t* buffer = malloc(region_size);
        if (!buffer) {
            free(pattern);
            free(regions);
            log_error("Failed to allocate memory for region buffer");
            lua_pushnil(L);
            return 1;
        }

        if (!validate_process_memory(p_pid, region.start, buffer, region_size)) {
            free(buffer);
            continue; // Continue to next region
        }

        for (size_t j = 0; j <= region_size - pattern_length; ++j) {
            if (match_pattern(buffer + j, pattern, pattern_length)) {
                // The resulting address is the start of the region
                // plus the index of the first byte that matches
                // plus the user-set offset, minus the process's base_address
                // or a subsequent memory read will read the wrong address or
                // go out of memory (due to commit 2b4417f offsetting memory reads)
                // So this result might be negative if the main module happens to be after
                // the found signature. This should be corrected by readAddress.
                intptr_t result = (region.start + j + offset) - process.base_address;

                free(buffer);
                free(pattern);
                free(regions);

                lua_pushnumber(L, result);
                return 1;
            }
        }

        free(buffer);
    }

    free(pattern);
    free(regions);

    // No match found
    log_error("No match found for the given signature");
    lua_pushnil(L);
    return 1;
}
