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
 * @return A borrowed pointer to the maps cache. The caller must not free it.
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
    if (pattern_len == 0 || haystack_len < pattern_len) {
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
    int ret = 1;
    MemoryIterator* mem_iter = NULL;
    SigByte* pattern = NULL;
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
        log_error("Failed to convert signature: invalid token or allocation failure");
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
    size_t overlap = pattern_length - 1;
    mem_iter = mem_iterator_new(p_pid, 0, 0, overlap);

    if (!mem_iter) {
        LOG_ERR("Memory iterator allocation failed, exiting signature scan.");
        lua_pushnil(L);
        goto cleanup;
    }

    // By construction, the memory iterator buffer size is MEMORY_WINDOW_SIZE
    if (pattern_length > mem_iter->buffer_size) {
        LOG_ERR("Memory signature provided is too large.");
        lua_pushnil(L);
        goto cleanup;
    }

    SigMatcher matcher;
    init_sig_matcher(&matcher, pattern, pattern_length);

    for (size_t i = 0; i < regions_count; i++) {
        ProcessMap region = regions[i];
        if (!mem_iterator_recycle(&mem_iter, p_pid, region.start, region.end, overlap)) {
            LOG_ERR("Unable to recycle memory iterator, exiting the sig_scan loop");
            lua_pushnil(L);
            goto cleanup;
        }
        uint8_t err = 0;
        while (mem_next(mem_iter, &err)) {
            if (mem_iter->buffer_size < pattern_length) {
                continue;
            }

            size_t found_index = 0;
            if (find_signature_in_buffer(
                    &matcher, mem_iter->buffer, mem_iter->buffer_size, &found_index)) {
                intptr_t result = (intptr_t)(mem_iter->last_cursor + found_index
                                      - process.base_address)
                    + offset;
                lua_pushnumber(L, result);
                goto cleanup;
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
