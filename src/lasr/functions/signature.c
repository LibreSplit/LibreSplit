#include "signature.h"

#include "../../logging.h"
#include "../maps/maps.h"
#include "../memory_iter/memory_iterator.h"
#include "../utils.h"

#include <fcntl.h>
#include <inttypes.h>
#include <lua.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
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
 * Gets all the memory regions of the monitored process
 *
 * @param[in] count A pointer to a counter onto where to store the number of regions
 *
 * @return A dinamically allocated array of ProcessMap that have been found
 */
ProcessMap* get_memory_regions(size_t* count)
{
    if (maps_cache == NULL || maps_cache_cycles == 0) {
        // maps_getAll clears the cache automatically before fillup
        *count = maps_getAll();
    } else {
        *count = maps_cache_size;
    }
    return maps_cache;
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
    if (token == NULL) {
        // Signature is all delimiters or empty
        free(signature_copy);
        return NULL;
    }
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
    int ret = 1;
    MemoryIterator* mem_iter = NULL;
    uint16_t* pattern = NULL;
    ProcessMap* regions = NULL;
    if (lua_gettop(L) != 2) {
        log_error("Invalid number of arguments: expected 2 (signature, offset)");
        lua_pushnil(L);
        goto cleanup;
    }

    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        log_error("Invalid argument types: expected (string, number)");
        lua_pushnil(L);
        goto cleanup;
    }

    pid_t p_pid = process.pid;
    const char* signature = lua_tostring(L, 1);
    intptr_t offset = lua_tointeger(L, 2);

    // Validate signature string
    if (strlen(signature) == 0) {
        log_error("Signature string cannot be empty");
        lua_pushnil(L);
        goto cleanup;
    }

    size_t pattern_length;
    pattern = convert_signature(signature, &pattern_length);
    if (!pattern) {
        log_error("Failed to convert signature");
        lua_pushnil(L);
        goto cleanup;
    }

    size_t regions_count = 0;
    regions = get_memory_regions(&regions_count);
    if (!regions) {
        log_error("Failed to get memory regions");
        lua_pushnil(L);
        goto cleanup;
    }

    // Forward initialization of the memory iterator.
    mem_iter = mem_iterator_new(p_pid, 0, 0, pattern_length);

    if (!mem_iter) {
        LOG_ERR("Memory iterator allocation failed, exiting signature scan.");
        goto cleanup;
    }

    // By construction, the memory iterator buffer size is MEMORY_WINDOW_SIZE
    if (pattern_length >= mem_iter->buffer_size) {
        LOG_ERR("Memory signature provided is too large.");
        lua_pushnil(L);
        goto cleanup;
    }

    for (size_t i = 0; i < regions_count; i++) {
        ProcessMap region = regions[i];
        if (!mem_iterator_recycle(&mem_iter, p_pid, region.start, region.end, pattern_length)) {
            LOG_ERR("Unable to recycle memory iterator, exiting the sig_scan loop");
            lua_pushnil(L);
            goto cleanup;
        }
        uint8_t err = 0;
        while (mem_next(mem_iter, &err)) {
            // Now buffer contains the read memory chunk
            for (size_t j = 0; j <= mem_iter->buffer_size - pattern_length; ++j) {
                // Since buffer_size and pattern_length are both size_t, the for loop condition
                // may underflow and not trigger if buffer_size < pattern_length
                if (mem_iter->buffer_size < pattern_length) {
                    continue;
                }
                if (match_pattern(mem_iter->buffer + j, pattern, pattern_length)) {
                    // The resulting address is the start of the region
                    // plus the index of the first byte that matches
                    // plus the user-set offset, minus the process's base_address
                    // or a subsequent memory read will read the wrong address or
                    // go out of memory (due to commit 2b4417f offsetting memory reads)
                    // So this result might be negative if the main module happens to be after
                    // the found signature. This should be corrected by readAddress.
                    intptr_t result = (mem_iter->last_cursor + j + offset) - process.base_address;

                    lua_pushnumber(L, result);
                    goto cleanup;
                }
            }
        }
        if (err == 3) {
            // Unreadable map
            continue;
        }
        if (err) {
            log_error("There has been an error in sig_scan: error code %d", err);
            lua_pushnil(L);
            goto cleanup;
        }
    }

    // No match found
    log_error("No match found for the given signature");
    lua_pushnil(L);
cleanup:
    if (maps_cache_cycles == 0) {
        // maps_clearCache takes care of freeing regions by itself.
        // if we do a free(regions) we'll run into a double-free problem.
        maps_clearCache();
    }
    free(pattern);
    pattern = NULL;
    mem_iterator_destroy(&mem_iter);
    return ret;
}
