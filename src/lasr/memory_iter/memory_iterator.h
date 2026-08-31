#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * Defines a Memory Iterator, keeping track
 * of a cursor in memory and used with other functions for
 * chunked reading.
 */
typedef struct _MemoryIterator {
    pid_t pid; /*!< ID of the process to be iterated over */
    uintptr_t start; /*!< Keeps the start address of the region to be read */
    uintptr_t end; /*!< Keeps the end address of the region to be read */
    uintptr_t last_cursor; /*!< The pointer to the beginning of the memory chunk to be scanned */
    uintptr_t cursor; /*!< The pointer to the beginning of the next memory chunk to be read */
    ptrdiff_t overlap; /*!< The overlap between two memory chunks to maintain */
    uint8_t* buffer;
    size_t buffer_size;
} MemoryIterator;

MemoryIterator* mem_iterator_new(pid_t pid, uintptr_t start, uintptr_t end, uintptr_t overlap);
int mem_next(MemoryIterator* iterator, uint8_t* err);
bool mem_iterator_destroy(MemoryIterator** iterator);
bool mem_iterator_recycle(MemoryIterator** iterator, pid_t pid, uintptr_t start, uintptr_t end, uintptr_t overlap);
