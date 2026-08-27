#include "export.h"
#include "auto-splitter.h"
// extern atomic_bool auto_splitter_running;
// ^^ TODO: Which is better?

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

lasr_global * shared_globals = NULL;

/**
 * (TODO doc)
 */
lasr_global * register_shared_global(char const * key)
{
	lasr_global * new;
	lasr_export value_;

	if (!key || atomic_load(&auto_splitter_running)) {
		/* Reject this call if the autosplitter is running
		 * The linked list is not atomic, so we are certain to "randomly"
		 * crash if this would happen */
		printf("Reject registration of export var `%s`\n", key ? key : "<none>");
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
	new->next = shared_globals;

	/* Attach the new value to the front of the linked list */
	shared_globals = new;

	return new;
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
int import_shared_global(lasr_global * import, lasr_export * target)
{
	int type = LASR_TYPE_INVALID;
	size_t len = 0;
	int import_state;
	int import_val;

	if (!import) {
		return LASR_TYPE_INVALID;
	}

	switch((import_state = atomic_load(&import->state))) {
		default:
			// sanity check (TODO: maybe remove me)
			printf("Unexpected state while loading: %d\n", atomic_load(&import->state));
			// fallthrough
		case LASR_STATE_NEEDED: // New data incompatible, no change (yet)
			atomic_store(&import->state, LASR_STATE_BORROWED);
			// fallthrough
		case LASR_STATE_BORROWED: // No change
			return LASR_TYPE_INVALID;

		case LASR_STATE_OWNED:
			if ((type = import->value.type) == LASR_TYPE_DYNAMIC) {
				/* TODO: For now, I've implemented this to resize every time,
				 * even if the new allocation is smaller. It might be better to
				 * only realloc if more space is needed, but this is hard to
				 * say without testing. */
				len = lasr_export_resize(target, import->value.dynamic->len);
				if (len > 0) {
					memcpy(target->dynamic->bytes, import->value.dynamic->bytes, len);
					target->type = type;
				}

			}
			else if (target) {
				(void) lasr_export_resize(target, 0);
				memcpy(&target->fixed, &import_val, sizeof(int));
				target->type = type;
			}

			// Pass this back to the lua thread
			atomic_store(&import->state, LASR_STATE_BORROWED);
			break;

		case LASR_STATE_ATOMIC: // Data is trustworthy
			type = import->value.type;
			import_val = atomic_load(&import->value.atomic);

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
 */
void lasr_export_release(lasr_export * value)
{
	if (value) {
		(void) lasr_export_resize(value, 0);
		free(value);
	}
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
		if (global->key) free((void *) global->key); /* strdup-ed on init */
		/* do nothing with `next` to avoid risk of accidental double-free */
		lasr_export_release((lasr_export *) &global->value);
		return global->next;
	}
	return NULL;
}
