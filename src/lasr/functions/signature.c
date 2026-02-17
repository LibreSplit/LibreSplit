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

#define SIG_SCAN_CHUNK_SIZE 0x10000

typedef struct SigByte {
    uint8_t value;
    uint8_t mask;
} SigByte;

typedef struct SigMatcher {
    const SigByte* signature;
    size_t signature_len;
    bool has_anchor;
    size_t anchor_pos;
    uint8_t anchor_byte;
    bool has_check;
    size_t check_pos;
    uint8_t check_byte;
} SigMatcher;

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
    // TODO: Convert this function to use maps.c functions
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
 * Tests whether one signature byte matches a target byte using the signature mask.
 *
 * @param[in] sig The signature byte and mask.
 * @param[in] byte The target byte.
 *
 * @return True if the byte matches after applying the mask, false otherwise.
 */
static bool sig_byte_matches(SigByte sig, uint8_t byte)
{
    return (byte & sig.mask) == sig.value;
}

static bool sig_matches_at(const uint8_t* haystack, size_t start, const SigByte* signature,
    size_t signature_len)
{
    for (size_t i = 0; i < signature_len; ++i) {
        if (!sig_byte_matches(signature[i], haystack[start + i])) {
            return false;
        }
    }

    return true;
}

static bool hex_nibble(char c, uint8_t* out)
{
    if (c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }

    if (c >= 'a' && c <= 'f') {
        *out = (uint8_t)(c - 'a' + 10);
        return true;
    }

    if (c >= 'A' && c <= 'F') {
        *out = (uint8_t)(c - 'A' + 10);
        return true;
    }

    return false;
}

static bool parse_sig_token(const char* token, SigByte* out)
{
    size_t length = strlen(token);
    if ((length == 1 && token[0] == '?') || (length == 2 && token[0] == '?' && token[1] == '?')) {
        out->value = 0;
        out->mask = 0;
        return true;
    }

    if (length != 2) {
        return false;
    }

    uint8_t hi_value = 0;
    uint8_t lo_value = 0;
    uint8_t hi_mask = 0;
    uint8_t lo_mask = 0;

    if (token[0] == '?') {
        hi_mask = 0;
    } else {
        if (!hex_nibble(token[0], &hi_value)) {
            return false;
        }
        hi_mask = 0xF;
    }

    if (token[1] == '?') {
        lo_mask = 0;
    } else {
        if (!hex_nibble(token[1], &lo_value)) {
            return false;
        }
        lo_mask = 0xF;
    }

    out->value = (uint8_t)((hi_value << 4) | lo_value);
    out->mask = (uint8_t)((hi_mask << 4) | lo_mask);
    return true;
}

/**
 * Converts an IDA-like signature string into a masked byte pattern.
 *
 * Supported token formats:
 * - Full-byte wildcard: `?`, `??`
 * - Full-byte exact: `AA`
 * - Nibble wildcard: `A?`, `?A`
 *
 * Tokens are split on ASCII whitespace.
 *
 * @param[in] signature A string containing the signature to convert.
 * @param[out] pattern_size A pointer onto where to save the size of the pattern.
 *
 * @return A dynamically allocated SigByte pattern, or NULL on parse/allocation failure.
 */
static SigByte* convert_signature(const char* signature, size_t* pattern_size)
{
    char* signature_copy = strdup(signature);
    if (!signature_copy) {
        return NULL;
    }

    char* token = strtok(signature_copy, " \t\r\n");
    size_t size = 0;
    size_t capacity = 10;
    SigByte* pattern = (SigByte*)malloc(capacity * sizeof(SigByte));
    if (!pattern) {
        free(signature_copy);
        return NULL;
    }

    while (token != NULL) {
        if (size >= capacity) {
            capacity *= 2;
            SigByte* temp = (SigByte*)realloc(pattern, capacity * sizeof(SigByte));
            if (!temp) {
                free(pattern);
                free(signature_copy);
                return NULL;
            }
            pattern = temp;
        }

        if (!parse_sig_token(token, &pattern[size])) {
            free(pattern);
            free(signature_copy);
            return NULL;
        }

        size++;
        token = strtok(NULL, " \t\r\n");
    }

    free(signature_copy);
    if (size == 0) {
        free(pattern);
        return NULL;
    }

    *pattern_size = size;
    return pattern;
}

static bool read_process_memory(pid_t pid, uintptr_t address, void* buffer, size_t size)
{
    struct iovec local_iov = { buffer, size };
    struct iovec remote_iov = { (void*)address, size };
    ssize_t nread = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);

    return nread == (ssize_t)size;
}

static void init_sig_matcher(SigMatcher* matcher, const SigByte* signature, size_t signature_len)
{
    matcher->signature = signature;
    matcher->signature_len = signature_len;
    matcher->has_anchor = false;
    matcher->anchor_pos = 0;
    matcher->anchor_byte = 0;
    matcher->has_check = false;
    matcher->check_pos = 0;
    matcher->check_byte = 0;

    for (size_t i = 0; i < signature_len; ++i) {
        if (signature[i].mask == 0xFF) {
            matcher->has_anchor = true;
            matcher->anchor_pos = i;
            matcher->anchor_byte = signature[i].value;
            break;
        }
    }

    if (!matcher->has_anchor) {
        return;
    }

    size_t best_distance = 0;
    for (size_t i = 0; i < signature_len; ++i) {
        if (i == matcher->anchor_pos || signature[i].mask != 0xFF) {
            continue;
        }

        size_t distance = (i > matcher->anchor_pos) ? (i - matcher->anchor_pos)
                                                    : (matcher->anchor_pos - i);

        if (!matcher->has_check || distance > best_distance) {
            matcher->has_check = true;
            matcher->check_pos = i;
            matcher->check_byte = signature[i].value;
            best_distance = distance;
        }
    }
}

static bool find_byte_swar(const uint8_t* haystack, size_t haystack_len, uint8_t needle,
    size_t start, size_t* found_index)
{
    if (start >= haystack_len) {
        return false;
    }

    const uint64_t ones = 0x0101010101010101ULL;
    const uint64_t highs = 0x8080808080808080ULL;
    uint64_t repeated = ((uint64_t)needle) * ones;

    while (start + sizeof(uint64_t) <= haystack_len) {
        uint64_t word;
        memcpy(&word, haystack + start, sizeof(word));

        uint64_t x = word ^ repeated;
        uint64_t eq = (x - ones) & (~x) & highs;
        if (eq != 0) {
            size_t byte_index = (size_t)(__builtin_ctzll(eq) / 8);
            *found_index = start + byte_index;
            return true;
        }

        start += sizeof(uint64_t);
    }

    for (size_t i = start; i < haystack_len; ++i) {
        if (haystack[i] == needle) {
            *found_index = i;
            return true;
        }
    }

    return false;
}

static bool find_signature_in_buffer(
    const SigMatcher* matcher, const uint8_t* haystack, size_t haystack_len, size_t* found_index)
{
    size_t pattern_len = matcher->signature_len;
    if (haystack_len < pattern_len) {
        return false;
    }

    if (matcher->has_anchor) {
        size_t search_from = matcher->anchor_pos;
        while (search_from < haystack_len) {
            size_t anchor_hit = 0;
            if (!find_byte_swar(haystack, haystack_len, matcher->anchor_byte, search_from,
                    &anchor_hit)) {
                return false;
            }

            size_t start = anchor_hit - matcher->anchor_pos;
            if (start + pattern_len > haystack_len) {
                return false;
            }

            if (matcher->has_check && haystack[start + matcher->check_pos] != matcher->check_byte) {
                search_from = anchor_hit + 1;
                continue;
            }

            if (sig_matches_at(haystack, start, matcher->signature, matcher->signature_len)) {
                *found_index = start;
                return true;
            }

            search_from = anchor_hit + 1;
        }

        return false;
    }

    for (size_t start = 0; start <= haystack_len - pattern_len; ++start) {
        if (sig_matches_at(haystack, start, matcher->signature, matcher->signature_len)) {
            *found_index = start;
            return true;
        }
    }

    return false;
}

/**
 * Scans process memory regions for a masked signature using chunked reads.
 *
 * Each region is read in fixed-size chunks with a trailing overlap window so
 * matches that cross chunk boundaries are still detected.
 *
 * @param[in] pid The process ID to scan.
 * @param[in] regions Memory regions to scan.
 * @param[in] regions_count Number of regions in the regions array.
 * @param[in] signature The parsed signature bytes.
 * @param[in] signature_len Number of bytes in signature.
 * @param[in] offset User-provided offset to add to the found address.
 * @param[out] result The final Lua-facing address offset by process base address.
 *
 * @return True if a match was found, false otherwise.
 */
static bool scan_signature_in_process(pid_t pid, const ProcessMap* regions, int regions_count,
    const SigByte* signature, size_t signature_len, intptr_t offset, intptr_t* result)
{
    uint8_t* chunk_buffer = malloc(SIG_SCAN_CHUNK_SIZE);
    uint8_t* prev_tail = malloc(signature_len > 1 ? signature_len - 1 : 1);
    uint8_t* window = malloc(SIG_SCAN_CHUNK_SIZE + (signature_len > 1 ? signature_len - 1 : 0));

    if (!chunk_buffer || !prev_tail || !window) {
        free(chunk_buffer);
        free(prev_tail);
        free(window);
        return false;
    }

    SigMatcher matcher;
    init_sig_matcher(&matcher, signature, signature_len);

    for (int i = 0; i < regions_count; ++i) {
        const ProcessMap region = regions[i];
        uintptr_t region_size = region.end - region.start;
        if (region_size == 0) {
            continue;
        }

        size_t prev_tail_len = 0;
        uintptr_t offset_bytes = 0;
        while (offset_bytes < region_size) {
            uintptr_t remaining = region_size - offset_bytes;
            size_t read_len = (remaining > SIG_SCAN_CHUNK_SIZE) ? SIG_SCAN_CHUNK_SIZE
                                                                : (size_t)remaining;

            if (!read_process_memory(pid, region.start + offset_bytes, chunk_buffer, read_len)) {
                break;
            }

            memcpy(window, prev_tail, prev_tail_len);
            memcpy(window + prev_tail_len, chunk_buffer, read_len);
            size_t window_len = prev_tail_len + read_len;

            size_t found_in_window = 0;
            if (find_signature_in_buffer(&matcher, window, window_len, &found_in_window)) {
                uintptr_t window_start = offset_bytes - prev_tail_len;
                uintptr_t found = window_start + found_in_window;
                *result = (intptr_t)((region.start + found + offset) - process.base_address);

                free(chunk_buffer);
                free(prev_tail);
                free(window);
                return true;
            }

            if (signature_len > 1) {
                prev_tail_len = (signature_len - 1 < read_len) ? (signature_len - 1) : read_len;
                memcpy(prev_tail, chunk_buffer + (read_len - prev_tail_len), prev_tail_len);
            }

            offset_bytes += read_len;
        }
    }

    free(chunk_buffer);
    free(prev_tail);
    free(window);
    return false;
}

/**
 * Performs the Lua Auto Splitter sig_scan function, pushing onto the Lua stack the result.
 *
 * Signature matching supports byte and nibble wildcards (for example `??`, `A?`, `?A`).
 *
 * If a pattern is found, the returned value is offset by process.base_address, allowing
 * the result to be used directly in readAddress without a module definition.
 *
 * Using readAddress with a module name and an address coming from sig_scan is not supported
 * and may result in out-of-process reads or other unforeseen consequences.
 *
 * @param L The lua state.
 *
 * @return Always 1 (one value is pushed on the stack: address or nil).
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
    SigByte* pattern = convert_signature(signature, &pattern_length);
    if (!pattern) {
        log_error("Failed to convert signature: invalid token or allocation failure");
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

    intptr_t result = 0;
    if (scan_signature_in_process(
            p_pid, regions, regions_count, pattern, pattern_length, offset, &result)) {
        free(pattern);
        free(regions);

        lua_pushnumber(L, result);
        return 1;
    }

    free(pattern);
    free(regions);

    // No match found
    log_error("No match found for the given signature");
    lua_pushnil(L);
    return 1;
}
