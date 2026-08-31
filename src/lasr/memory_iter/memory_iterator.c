#include "memory_iterator.h"
#include "src/lasr/utils.h"
#include "src/logging.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const size_t MEMORY_WINDOW_SIZE = 0x10000; /*!< The size of the memory chunks to be read*/

/**
 * Creates a new chunked Memory Iterator.
 *
 * @param[in] pid The process id to scan for.
 * @param[in] start The start address to start the scan from.
 * @param[in] end The end address onto where to finish the scan.
 * @param[in] overlap The amount of overlap between two successive iterations.
 *
 * @returns A pointer to a new memory iterator.
 */
MemoryIterator* mem_iterator_new(pid_t pid, uintptr_t start, uintptr_t end, uintptr_t overlap)
{
    MemoryIterator* iter = malloc(sizeof(MemoryIterator));
    if (iter == NULL) {
        return NULL;
    }
    uint8_t* tmp = (uint8_t*)malloc(MEMORY_WINDOW_SIZE);
    if (tmp == NULL) {
        free(iter);
        return NULL;
    }
    iter->buffer = tmp;
    iter->pid = pid;
    iter->start = start;
    iter->end = end;
    iter->overlap = overlap;
    iter->cursor = start;
    iter->last_cursor = start;
    iter->buffer_size = MEMORY_WINDOW_SIZE;
    return iter;
}

/**
 * Reads the next chunk of memory.
 *
 * @param[in] iterator The pointer to the MemoryIterator used.
 * @param[out] err The error code returned to the calling function.
 *
 * @returns 1 if there has been a memory read, 0 otherwise.
 */
int mem_next(MemoryIterator* iterator, uint8_t* err)
{
    assert(iterator != NULL);
    assert(iterator->cursor != 0);
    assert(iterator->cursor >= iterator->start);
    size_t window_size = MEMORY_WINDOW_SIZE;
    bool last_iter = false;
    if (iterator->cursor >= iterator->end) {
        // We passed the end in the last iteration. Stop.
        return 0;
    }
    if (iterator->cursor + window_size > iterator->end) {
        // We're at the last iteration and the window size is too big
        window_size = (size_t)(iterator->end - iterator->cursor);
        last_iter = true;
        if (window_size == 0) {
            // Nothing more to read
            return 0;
        }
    }
    // Read memory
    struct iovec local_iov = { iterator->buffer, window_size };
    struct iovec remote_iov = { (void*)iterator->cursor, window_size };
    ssize_t nread = process_vm_readv(iterator->pid, &local_iov, 1, &remote_iov, 1, 0);
    if (nread == (ssize_t)window_size) {
        iterator->last_cursor = iterator->cursor;
        // If read is okay, move the cursor forward
        iterator->cursor += window_size;
        if (!last_iter) {
            // If we're not done, move it backwards a little to account for
            // Matches between chunks
            iterator->cursor -= iterator->overlap;
        }
        iterator->buffer_size = window_size;
        // There are more things to read
        return 1;
    } else {
        if (nread < 0) {
            if (errno == EINTR) {
                // TODO: [Penaz] [2026-04-02] Memory read interrupted by a transitory interruption.
                // ^ For now we'll still bail out. but ideally we should retry.
                LOG_DEBUG("Read interrupted by EINTR");
            }
            LOG_DEBUGF("Memory read error, errno is %d (%s)", errno, strerror(errno));
            LOG_DEBUGF("Cursor: %x, Start: %x, End: %x, Overlap: %x", iterator->cursor, iterator->start, iterator->end, iterator->overlap);
            *err = 3;
        } else if (nread == 0) {
            // No bytes read. We stop here
            iterator->buffer_size = 0;
            return 0;
        } else {
            // Short read
            LOG_WARNF("Short memory read: nread=%zd, expected=%td", nread, window_size);
            // Save the short read cursor
            iterator->last_cursor = iterator->cursor;
            // Advance by the number of bytes read
            iterator->buffer_size = (size_t)nread;
            iterator->cursor += (size_t)nread;
            if (!last_iter) {
                // If we're not done, move it backwards a little to account for
                // Matches between chunks.
                if (nread > iterator->overlap) {
                    // If the short read is longer than the overlap, we try to continue
                    iterator->cursor -= iterator->overlap;
                } else {
                    // If it is shorter, we bail and hope for the next loop
                    return 0;
                }
            }
            return 1;
        }
    }
    // Nothing more to read or error has happened.
    return 0;
}

bool mem_iterator_destroy(MemoryIterator** iterator)
{
    if (*iterator != NULL) {
        free((*iterator)->buffer);
        (*iterator)->buffer = NULL;
        free(*iterator);
        *iterator = NULL;
    }
    return true;
}

/**
 * Reinitialies an existing MemoryIterator to avoid useless reinstantiations.
 *
 * This works on quite a big assumption: that the buffer inside of the MemoryIterator
 * is MEMORY_WINDOW_SIZE big at allocation time. This should be a given if MemoryIterator
 * was created via mem_iterator_new().
 *
 * @param[inout] iter The reference to the Memory Iterator to recycle.
 * @param[in] pid The process ID to tie the MemoryIterator to.
 * @param[in] start The start address to start the scan from.
 * @param[in] end The end address onto where to finish the scan.
 * @param[in] overlap The amount of overlap between two successive iterations.
 */
bool mem_iterator_recycle(MemoryIterator** iter, pid_t pid, uintptr_t start, uintptr_t end, uintptr_t overlap)
{
    if (*iter == NULL) {
        LOG_ERR("Cannot recycle NULL pointers.");
        return false;
    }
    if ((*iter)->buffer == NULL) {
        LOG_ERR("Cannot recycle a memory iterator with a NULL buffer");
        return false;
    }
    (*iter)->pid = pid;
    (*iter)->start = start;
    (*iter)->end = end;
    (*iter)->overlap = overlap;
    (*iter)->cursor = start;
    (*iter)->last_cursor = start;
    (*iter)->buffer_size = MEMORY_WINDOW_SIZE;
    return true;
}
