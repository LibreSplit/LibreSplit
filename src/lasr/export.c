/** \file export.c
 *
 * Functions for handling the exchange of shared global values between the
 * lua auto-splitter runtime thread and the main timer thread.
 */

#include "export.h"

#include "../logging.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Allocates and initializes a new `lasr_global` type for data shared between
 * a reading and writing thread.
 *
 * @param key C-string indicating the name of the Lua variable that this
 * instance will track.
 *
 * @return A pointer to the new instance. Returned value should be freed with
 * lasr_global_release()
 */
lasr_global* lasr_global_create(char const* key)
{
    lasr_global* new;
    lasr_export value_;

    if (!key) {
        return NULL;
    }

    new = malloc(sizeof(lasr_global));
    if (!new) {
        return NULL;
    }

    new->key = strdup(key);
    if (!new->key) {
        free(new);
        return NULL;
    }

    atomic_store(&new->state, LASR_STATE_ATOMIC);
    value_.type = LASR_TYPE_NIL;
    value_.fixed = 0;

    memcpy(&new->value, &value_, sizeof(lasr_value));
    return new;
}

/**
 * Helper function that will store an "incoming" atomic-width value into a
 * 'lasr_global' export container. This function should only be invoked by the
 * WRITEing thread.
 *
 * Handles memory reallocation, including the case when the previously stored
 * value was dynamically allocated.
 *
 * @param container A non-null reference to a 'lasr_global' container. This is
 * the data destination.
 *
 * @param value The new value to be written. The value may be cast, but the
 * READer must know the expected type if so.
 *
 * @param type The lasr type of the new value to be written; must be an atomic
 * type (not DYNAMIC.)
 */
void export_atomic_global(lasr_global* container, int const value, int const type)
{
    int container_state;
    int container_type;

    if (type == LASR_TYPE_INVALID || type == LASR_TYPE_DYNAMIC) {
        LOG_DEBUG("Call to store non-atomic value as atomic");
        return;
    }

    container_state = atomic_load(&container->state);
    container_type = atomic_load(&container->value.type);

    /* Asserted by auto-splitter.c:update_shared_globals() (caller)
     * PR TODO: Would this function be improved by making it a static symbol
     * in autosplitter.c?
    if (container_state == LASR_STATE_NEEDED || container_state == LASR_STATE_OWNED) {
            return;
    }
     */

    switch (container_type) {
        case LASR_TYPE_NIL:
            if (type != LASR_TYPE_NIL) {
                /* nil -> value: store value, then store type */
                atomic_store(&container->value.atomic, value);
                atomic_store(&container->value.type, type);
            }
            /* (else) nil -> nil: do nothing */
            break;

        case LASR_TYPE_ATOMIC:
            if (type != LASR_TYPE_NIL) {
                /* value -> value: store value */
                atomic_store(&container->value.atomic, value);
            } else {
                /* value -> nil: store type only */
                atomic_store(&container->value.type, type);
            }
            break;

        case LASR_TYPE_DYNAMIC:
        default:
            /* Conversion inbound, full ownership needed */
            if (container_state != LASR_STATE_BORROWED) {
                atomic_store(&container->state, LASR_STATE_NEEDED);
                return;
            }

            if (container->value.dynamic == NULL) {
                /* sanity assertion -- possible if type was INVALID but that should
                 * never happen, either */
                LOG_DEBUGF("String data `%s` is NULL (it shouldn't be)", container->key);
            }

            (void)lasr_export_resize((lasr_export*)&container->value, 0);
            atomic_store(&container->value.type, type);

            if (type != LASR_TYPE_NIL) {
                atomic_store(&container->value.atomic, value);
            } else {
                /* Store a zero because the old data (a pointer) is certainly
                 * invalid. Note that BORROWED state is asserted here. */
                atomic_store(&container->value.atomic, 0);
            }
            break;
    }

    if (container_state == LASR_STATE_BORROWED) {
        atomic_store(&container->state, LASR_STATE_ATOMIC);
    }
}

/**
 * Helper function that will store an "incoming" dynamically-sized value into a
 * 'lasr_global' export container. This function should only be invoked by the
 * WRITEing thread.
 *
 * Handles memory reallocation, including the case when the previously stored
 * value had no allocation. Exits early if old data is the same as the new.
 *
 * @param container A non-null reference to a 'lasr_global' container. This is
 * the data destination.
 *
 * @param value Reference to the new data to be written. Will always be NUL-
 * terminated, even if data is a binary string.
 *
 * @param len Number of bytes in the new value, excluding a NUL byte.
 */
void export_dynamic_global(lasr_global* container, char const* const value, size_t const len)
{
    size_t new_len = len;
    lasr_export* container_value = (lasr_export*)&container->value;
    int container_state = atomic_load(&container->state);

    if (container_state != LASR_STATE_BORROWED) {
        atomic_store(&container->state, LASR_STATE_NEEDED);
        return;
    }

    if (container_value->type != LASR_TYPE_DYNAMIC) {
        container_value->dynamic = NULL; /* flag for allocation */
        container_value->type = LASR_TYPE_DYNAMIC;
    }

    if ((!container_value->dynamic) || (container_value->dynamic->len != len)) {
        /* Strings are trivially different
         * new_len is possibly smaller than len if realloc failed with ENOMEM */
        new_len = lasr_export_resize(container_value, len + 1);

        if (new_len == 0) {
            return;
        }
    } else if (memcmp(container_value->dynamic->bytes, value, new_len)) {
        /* fallthrough block -- strings are different */
        (void)0;
    } else {
        /* Strings are the same, nothing to do.
         * Timer thread reallocs on every read, so we don't want to pass
         * this back and forth more than necessary. */
        return;
    }

    memcpy(container_value->dynamic->bytes, value, new_len);
    container_value->dynamic->bytes[new_len - 1] = '\0'; /* Enforce NUL byte */
    atomic_store(&container->state, LASR_STATE_OWNED);
}

/**
 * Retrieves a shared value exported by the timer into a buffer.
 *
 * @param container A non-null reference to a 'lasr_global' container. This is
 * the data source.
 *
 * @param target A non-null reference to a 'lasr_export' value. This is the
 * data destination. Memory referenced by this type will be allocated or freed
 * as necessary, indicated by the value of target->type after return. However,
 * it must later be released by the caller if target is not in static storage.
 *
 * @return The type that was stored. This is returned even if there is no
 * target to write to so that the state machine may continue even if the output
 * is not needed.
 */
int import_shared_global(lasr_global* container, lasr_export* target)
{
    size_t len = 0;

    int container_state;
    int container_type;

    if (!container) {
        return LASR_TYPE_INVALID;
    }

    container_type = atomic_load(&container->value.type);
    container_state = atomic_load(&container->state);

    switch (container_state) {
        default:
            LOG_DEBUGF("Unexpected state while loading: %d", container_state);
            // fallthrough
        case LASR_STATE_NEEDED:
            /* New data incompatible, no change (yet) */
            atomic_store(&container->state, LASR_STATE_BORROWED);
            // fallthrough
        case LASR_STATE_BORROWED:
            /* No change */
            return LASR_TYPE_INVALID;

        case LASR_STATE_ATOMIC:
            if (container_type == LASR_TYPE_DYNAMIC) {
                LOG_DEBUGF("Invalid export `%s` -- state is atomic, but data is variable", container->key);
                return LASR_TYPE_INVALID;
            }
            // fallthrough
        case LASR_STATE_OWNED:
            if (target) {
                switch (container_type) {
                    case LASR_TYPE_DYNAMIC:
                        len = lasr_export_resize(target, container->value.dynamic->len);
                        if (len > 0) {
                            memcpy(target->dynamic->bytes, container->value.dynamic->bytes, len);
                            target->type = LASR_TYPE_DYNAMIC;
                        }
                        break; /* nested switch */

                    case LASR_TYPE_NIL:
                        (void)lasr_export_resize(target, 0);
                        target->fixed = 0;
                        target->type = LASR_TYPE_NIL;
                        break;

                    case LASR_TYPE_ATOMIC:
                        (void)lasr_export_resize(target, 0);
                        target->fixed = atomic_load(&container->value.atomic);
                        target->type = LASR_TYPE_ATOMIC;
                        break; /* nested switch */
                }
            }

            /* Pass this back to the lua thread, atomic otherwise */
            if (container_state == LASR_STATE_OWNED) {
                atomic_store(&container->state, LASR_STATE_BORROWED);
            }
            break;
    }
    return container_type;
}

/**
 * Reallocate a 'lasr_export' value to support dynamic data of a given length.
 *
 * @param value A non-null reference to a 'lasr_export' value to be resized.
 *
 * @param len Number of bytes in the array to be stored. If nonzero, the actual
 * allocated size will be adjusted as necessary to accomidate the prefix.
 * Otherwise, when zero, the dynamic data will be freed and the export value
 * can be used to safely store fixed-length values.
 *
 * @return New length of the dynamic data (ideally always the same as len.)
 * This value is possibly smaller than len if realloc() fails, but never
 * larger.
 */
size_t lasr_export_resize(lasr_export* value, size_t len)
{
    size_t total_size;

    if (!value) {
        return 0;
    }

    if (len == 0) {
        if (value->type == LASR_TYPE_DYNAMIC) {
            free(value->dynamic);
            value->dynamic = NULL;
            value->type = LASR_TYPE_NIL;
        }
        return 0;
    }

    /* sizeof(lasr_dynamic_data) === sizeof(size_t)
     * ^^ sizeof(lasr_dynamic_data.bytes) is 0 */
    errno = 0;
    total_size = len + sizeof(lasr_dynamic_data);
    value->dynamic = realloc(value->dynamic, total_size);
    if (errno == 0) {
        /* Success: update new len */
        value->dynamic->len = len;
    } else {
        /* Fail: reset len to old */
        len = value->dynamic->len;
    }
    return len;
}

/**
 * Safely frees a 'lasr_global' container, including any nested allocations.
 *
 * @param container A non-null reference to a 'lasr_global' container that will
 * be released.
 */
void lasr_global_release(lasr_global* global)
{
    if (global) {
        if (global->key) {
            free((void*)global->key); /* strdup-ed on init */
        }
        lasr_export_resize((lasr_export*)&global->value, 0);
        /* do nothing with `next` to avoid risk of accidental double-free */
    }
}
