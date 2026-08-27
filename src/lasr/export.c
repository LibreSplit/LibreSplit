#include "export.h"

#include "../logging.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
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
lasr_global * lasr_global_create(char const * key)
{
	lasr_global * new;
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
void export_atomic_global(lasr_global * container, int const value, int const type)
{
	// TODO: Assert type (fix this when correcting the nil check)

	int container_state = atomic_load(&container->state);
	if (container_state == LASR_STATE_NEEDED) {
		return;
	}
	else if (container_state == LASR_STATE_OWNED) {
		atomic_store(&container->state, LASR_STATE_NEEDED);
		return;
	}

	if (container->value.type == LASR_TYPE_DYNAMIC) {
		/* ^^ old is dynamic */
		if (container->value.dynamic == NULL) {
			/* Sanity check assertion */
			LOG_DEBUGF("String data `%s` is NULL (it shouldn't be)", container->key);
		} 

		(void) lasr_export_resize((lasr_export *) &container->value, 0);
		((lasr_export *) &container->value)->type = type;
	}

	atomic_store(&container->value.atomic, value);
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
void export_dynamic_global(lasr_global * container, char const * const value, size_t const len)
{
	size_t new_len = len;
	int container_state = atomic_load(&container->state);
	if (container_state != LASR_STATE_BORROWED) {
		atomic_store(&container->state, LASR_STATE_NEEDED);
		return;
	}

	if (container->value.type != LASR_TYPE_DYNAMIC) {
		/* ^^ old is atomic; convert it now that we have control */
		((lasr_export *) &container->value)->type = LASR_TYPE_DYNAMIC;
		((lasr_export *) &container->value)->dynamic = NULL; /* flag for allocation */
	}

	if ((!container->value.dynamic)
		|| (container->value.dynamic->len != len)) {
		/* Strings are trivially different
		 * new_len is possibly smaller than len if realloc failed with ENOMEM */
		new_len = lasr_export_resize((lasr_export *) &container->value, len + 1);

		if (new_len == 0) {
			return;
		}
	} 
	else if (memcmp(&container->value.dynamic->bytes, value, new_len)) {
		/* fallthrough block -- strings are different */
		(void) 0;
	}
	else {
		/* Strings are the same, nothing to do.
		 * Timer thread reallocs on every read, so we don't want to pass
		 * this back and forth more than necessary. */
		return;
	}

	memcpy(((lasr_export *) &container->value)->dynamic->bytes, value, new_len);
	((lasr_export *) &container->value)->dynamic->bytes[new_len - 1] = '\0'; /* Enforce NUL byte */
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
int import_shared_global(lasr_global * container, lasr_export * target)
{
	int type = LASR_TYPE_INVALID;
	size_t len = 0;
	int import_state;
	int import_val;

	if (!container) {
		return LASR_TYPE_INVALID;
	}

	switch((import_state = atomic_load(&container->state))) {
		default:
			LOG_DEBUGF("Unexpected state while loading: %d", atomic_load(&container->state));
			// fallthrough
		case LASR_STATE_NEEDED: // New data incompatible, no change (yet)
			atomic_store(&container->state, LASR_STATE_BORROWED);
			// fallthrough
		case LASR_STATE_BORROWED: // No change
			return LASR_TYPE_INVALID;

		case LASR_STATE_OWNED:
			if ((type = container->value.type) == LASR_TYPE_DYNAMIC) {
				/* TODO: For now, I've implemented this to resize every time,
				 * even if the new allocation is smaller. It might be better to
				 * only realloc if more space is needed, but this is hard to
				 * say without testing. */
				len = lasr_export_resize(target, container->value.dynamic->len);
				if (len > 0) {
					memcpy(target->dynamic->bytes, container->value.dynamic->bytes, len);
					target->type = type;
				}

			}
			else if (target) {
				(void) lasr_export_resize(target, 0);
				memcpy(&target->fixed, &import_val, sizeof(int));
				target->type = type;
			}

			// Pass this back to the lua thread
			atomic_store(&container->state, LASR_STATE_BORROWED);
			break;

		case LASR_STATE_ATOMIC: // Data is trustworthy
			type = container->value.type;
			import_val = atomic_load(&container->value.atomic);

			if (target) {
				(void) lasr_export_resize(target, 0);
				memcpy(&target->fixed, &import_val, sizeof(int));
				/* NOTE: type cannot be changed in the atomic state */
			}
			break;

	}
	return type;
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
size_t lasr_export_resize(lasr_export * value, size_t len)
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
	}
	else {
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
void lasr_global_release(lasr_global * global)
{
	if (global) {
		if (global->key) {
			free((void *) global->key); /* strdup-ed on init */
		}
		lasr_export_resize((lasr_export *) &global->value, 0);
		/* do nothing with `next` to avoid risk of accidental double-free */
	}
}
