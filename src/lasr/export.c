#include "export.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * (TODO doc)
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

void export_atomic_global(lasr_global * container, int const value, int const type)
{
	int container_state = atomic_load(&container->state);
	if (container_state != LASR_STATE_BORROWED
		|| container_state != LASR_STATE_ATOMIC) {
		atomic_store(&container->state, LASR_STATE_NEEDED);
		return;
	}

	if (container->value.type == LASR_TYPE_DYNAMIC) {
		/* ^^ old is dynamic */
		if (container->value.dynamic == NULL) {
			// debugging sanity check (TODO: cleanup)
			printf("string data `%s` is NULL (it shouldn't be)\n", container->key);
		} 

		(void) lasr_export_resize((lasr_export *) &container->value, 0);
		((lasr_export *) &container->value)->type = type;
	}


	atomic_store(&container->value.atomic, value);
	if (container_state == LASR_STATE_BORROWED) {
		atomic_store(&container->state, LASR_STATE_ATOMIC);
	}
}

/* NOTE: This should only be called by the writing thread...maybe best to put
 * it with the autosplitter logic, but the state machine is easier to see with
 * all of the meat in one place. */
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

 * Also, naming is hard. Import implies it's a one-time thing sorta, but I also
 * want to indicate that this call is the "other half" of the state machine and
 * therefore important.

 * TODO: Not fully sure if this is the best place for this implementation, but
 * this location follows suit with restart_auto_splitter being an operation
 * that lets the timer thread interact with the splitter thread.

 * *Also* TODO: This docu-string.

 * Returns the type that was stored.
 * For now, this is returned even if there is no target to write to.
 * The state machine may continue even if the output is not needed.

 * New goal: lasr_export should be fully self-contained.
 * Memory can be allocated, but it should have all the information needed
 * for cleanup/realoc, with respect to type and size information.
 * This will probably be best-suited for helper functions.
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
			// sanity check (TODO: maybe remove me)
			printf("Unexpected state while loading: %d\n", atomic_load(&container->state));
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
 * NOTE: Like realloc, a size of 0 will free the contents.
 * NOTE: We always trust the associated type in this operation.
 * If it is corrupt, then that is a bug elsewhere.
 *
 * size: number of bytes of dynamic data
 * return: new length of the dynamic data (ideally always the same as ^^ )
 *
 * (TODO doc)
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
 * (TODO doc)
 *
 * Returns the value of next
 *
 * NOTE: There is no check for the current atomic state, caller to assert that
 */
lasr_global * lasr_global_release(lasr_global * global)
{
	if (global) {
		if (global->key) {
			free((void *) global->key); /* strdup-ed on init */
		}
		lasr_export_resize((lasr_export *) &global->value, 0);

		/* do nothing with `next` to avoid risk of accidental double-free */
		return global->next;
	}
	return NULL;
}
