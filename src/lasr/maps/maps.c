#include "maps.h"

#include "src/lasr/utils.h"

#include <fcntl.h>
#include <linux/fs.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

ProcessMap* maps_cache = NULL; // Array of cached maps
size_t maps_cache_size = 0; // Number of cached maps

// Use blocks to build the maps cache incrementally
// This allows the cache to be built without knowing the total size in advance
// Also makes it easier to manage memory so we dont overshoot in alloc size and grows predictably
static MapsBlock* head = NULL; // Head of blocks
static MapsBlock* current = NULL; // Current block being filled

/**
 * Append a ProcessMap entry to the internal block list.
 * @param e The ProcessMap entry to append.
 */
static void append_entry(ProcessMap e)
{
    if (!current || current->used == MAPS_CACHE_BLOCK_SIZE) {
        MapsBlock* new_block = malloc(sizeof(MapsBlock));
        if (!new_block) {
            perror("Failed to allocate memory for maps block");
            exit(EXIT_FAILURE);
        }

        new_block->used = 0;
        new_block->next = NULL;

        if (!head)
            head = new_block;
        else
            current->next = new_block;

        current = new_block;
    }

    current->entries[current->used++] = e;
}
/**
 * Flatten the internal linked list of MapsBlock into a contiguous array.
 * @param out_count Pointer to a size_t which receives the number of entries.
 *
 * Internal linked list of MapsBlock is left untouched, still being valid
 *
 * Returns: pointer to malloc'd array of ProcessMap entries.
 */
static ProcessMap* maps_flatten(size_t* out_count)
{
    size_t total = 0;
    for (MapsBlock* b = head; b; b = b->next)
        total += b->used;

    ProcessMap* arr = malloc(total * sizeof(ProcessMap));
    if (!arr)
        abort();

    size_t idx = 0;
    for (MapsBlock* b = head; b; b = b->next) {
        memcpy(&arr[idx], b->entries, b->used * sizeof(ProcessMap));
        idx += b->used;
    }

    *out_count = total;
    return arr;
}

/**
 * Free and clear the maps cache and internal block list.
 *
 * For use before rebuilding the cache from scratch
 *
 * Frees all allocated MapsBlock structures used for incremental collection
 * and frees the flattened `maps_cache` array if present. Resets the
 * `head`, `current`, and `maps_cache_size` to their empty states.
 */
void maps_clearCache(void)
{
    MapsBlock* b = head;
    while (b) {
        MapsBlock* next = b->next;
        free(b);
        b = next;
    }
    head = NULL;
    current = NULL;

    if (maps_cache) {
        free(maps_cache);
        maps_cache = NULL;
        maps_cache_size = 0;
    }
}

#ifdef IOCTL_MAPS
/**
 * Check if PROCMAP_QUERY ioctl is supported on the current system.
 *
 * Opens /proc/self/maps and attempts to issue a PROCMAP_QUERY ioctl.
 * If the ioctl returns >= 0, it is supported.
 *
 * @return true if supported, false otherwise.
 */
static bool maps_ioctlSupported(void)
{
    const char* path = "/proc/self/maps";
    int f = open(path, O_RDONLY);
    if (f < 0) {
        return false;
    }

    struct procmap_query q = { 0 };
    q.size = sizeof(q);
    q.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
    q.query_addr = 0;

    int ret = ioctl(f, PROCMAP_QUERY, &q);
    close(f);
    return ret >= 0;
}

/**
 * Populate the maps cache by querying the target process maps.
 *
 * uses `ioctl(PROCMAP_QUERY)` in a loop to collect all VMA information.
 * Entries are collected into internal blocks and then flattened into `maps_cache`.
 *
 * @return Number of maps collected
 */
static size_t maps_getAll_ioctl(void)
{
    char path[22]; // 22 is the maximum length the path can be (strlen("/proc/4294967296/maps"))
    snprintf(path, sizeof(path), "/proc/%d/maps", process.pid);
    int f = open(path, O_RDONLY);
    if (f >= 0) {
        struct procmap_query q = { 0 };
        char map_name[PATH_MAX] = { 0 };
        q.size = sizeof(q);
        q.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
        q.query_addr = 0;
        maps_clearCache();
        for (;;) {
            q.vma_name_addr = (uintptr_t)map_name;
            q.vma_name_size = sizeof(map_name);
            int ret = ioctl(f, PROCMAP_QUERY, &q);
            if (ret < 0) {
                break;
            }
            ProcessMap map = {
                .start = q.vma_start,
                .end = q.vma_end,
                .size = q.vma_end - q.vma_start,
            };
            strncpy(map.name, q.vma_name_addr ? map_name : "", sizeof(map.name));
            map.name[sizeof(map.name) - 1] = '\0';
            map_name[0] = '\0';
            append_entry(map);
            // Advance past this mapping
            q.query_addr = q.vma_end;
        }
        close(f);
        maps_cache = maps_flatten(&maps_cache_size);
    }
    return maps_cache_size;
}

#endif

/**
 * Parse a single line from /proc/[pid]/maps into a ProcessMap structure.
 * @param line The line to parse.
 * @param map Pointer to ProcessMap to receive the parsed data.
 *
 * @return true on successful parse, false otherwise.
 */
static bool maps_parseMapsLine(const char* line, ProcessMap* map)
{
    uint64_t size;
    char mode[8];
    unsigned long offset;
    unsigned int major_id, minor_id, node_id;

    // Thank you kernel source code
    int sscanf_res = sscanf(line, "%lx-%lx %7s %lx %u:%u %u %" STR(PATH_MAX) "[^\n]", &map->start,
        &map->end, mode, &offset, &major_id,
        &minor_id, &node_id, map->name);
    if (!sscanf_res)
        return false;

    // Calculate the map size
    size = map->end - map->start;
    map->size = size;
    return true;
}

/**
 * Populate the maps cache by reading /proc/[pid]/maps.
 *
 * @return Number of maps collected
 */
static size_t maps_getAll_legacy(void)
{
    char path[22]; // 22 is the maximum length the path can be (strlen("/proc/4294967296/maps"))

    snprintf(path, sizeof(path), "/proc/%d/maps", process.pid);

    FILE* f = fopen(path, "r");

    if (f) {
        char current_line[PATH_MAX + 100];
        maps_clearCache();
        while (fgets(current_line, sizeof(current_line), f) != NULL) {
            ProcessMap map;
            if (maps_parseMapsLine(current_line, &map)) {
                append_entry(map);
            } else {
                printf("Failed to parse maps line: %s\n", current_line);
            }
        }
        fclose(f);
        maps_cache = maps_flatten(&maps_cache_size);
    }
    return maps_cache_size;
}

#ifdef IOCTL_MAPS
static size_t maps_getAll_init(void);
static size_t (*maps_getAll_var)(void) = maps_getAll_init;
#else
static size_t (*maps_getAll_var)(void) = maps_getAll_legacy;
#endif

#ifdef IOCTL_MAPS
/**
 * Initialize the maps_getAll function pointer based on ioctl support.
 * Also calls the selected function to populate the maps cache initially.
 * @return Number of maps collected
 */
static size_t maps_getAll_init(void)
{
    if (!getenv("LIBRESPLIT_DISABLE_IOCTL_MAPS") && maps_ioctlSupported()) {
        maps_getAll_var = maps_getAll_ioctl;
        printf("PROCMAP_QUERY is supported, using ioctl method for maps retrieval.\n");
    } else {
        maps_getAll_var = maps_getAll_legacy;
        printf("PROCMAP_QUERY is not supported, using legacy method for maps retrieval.\n");
    }
    return (*maps_getAll_var)();
}
#endif

/**
 * Get all process maps and populate the maps cache.
 *
 * @return Number of maps collected
 */
size_t maps_getAll(void)
{
    return (*maps_getAll_var)();
}

/**
 * Find a map by substring match on its name.
 * @param name Substring to search for (must not be NULL).
 * @param out_map Pointer to ProcessMap to receive result on success.
 *
 * Searches the current `maps_cache` for an entry whose name contains
 * the provided substring. If no entry is found, the cache is refreshed via
 * `maps_getAll()` and the search is retried. On success the matching
 * ProcessMap is copied into `out_map`
 *
 * Returns: true if a matching map was found, false otherwise.
 */
bool maps_findMapByName(const char* name, ProcessMap* out_map)
{
    if (!name)
        return false;

    for (uint32_t i = 0; i < maps_cache_size; i++) {
        const char* map_name = maps_cache[i].name;
        if (strstr(map_name, name) != NULL) {
            *out_map = maps_cache[i];
            return true;
        }
    }

    // We didnt find it, get
    maps_getAll();

    for (uint32_t i = 0; i < maps_cache_size; i++) {
        const char* map_name = maps_cache[i].name;
        if (strstr(map_name, name) != NULL) {
            *out_map = maps_cache[i];
            if (!maps_cache_cycles) { // Cache is disabled, clear after use
                maps_clearCache();
            }
            return true;
        }
    }

    return false;
}
