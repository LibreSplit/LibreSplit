#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#define LASR_STATE_ATOMIC	-1  /* Value is shared via atomic instructions */
#define LASR_STATE_BORROWED	 0  /* Write thread has full control */
#define LASR_STATE_OWNED	 1  /* Read thread has full control */
#define LASR_STATE_NEEDED	 2  /* Write thread requesting full control */

#define LASR_TYPE_INVALID	 0  /* Corresponding data cannot be resolved and should be ignored */
#define LASR_TYPE_NIL		 1  /* There exists no data (null, none, nil, etc.) */
#define LASR_TYPE_ATOMIC	 2  /* "Simple" data that can be read with atomic instructions */
#define LASR_TYPE_DYNAMIC	 3  /* "Array" data that requires stricter thread safety */
// TODO: Originally I planned more graunular types like float, double, time,
// etc., but my initial implementation ultimately shadowed the above.
// Should it be left to the components to assume/interpret the type they expect?

/**
 * Lua string type representation (struct extends)
 *
 * TODO: Might nest this within shared_data to be less confusing which is which
 *
 */
typedef struct {
	size_t len;
	char bytes[];
} lasr_dynamic_data;

// TODO: Consider renaming me??
typedef struct {
	int const type; /* data type (LASR_TYPE_XXX) */
	union {
		atomic_int atomic;
		lasr_dynamic_data const * const dynamic;
	};
} lasr_value;

/* shadow of lasr_value but for asserted thread-safe use */
typedef struct {
	int type;
	union {
		int fixed;
		lasr_dynamic_data * dynamic;
	};
} lasr_export;

/**
 * Borrowed data base type. Owned by READing thread.
 *
 * (todo docucomment)
 */
typedef struct _lasr_global lasr_global;
struct _lasr_global {
    char const * key; /* consider `name` ?? not sure which is more intuitive */
	lasr_global * next; /* linked list next */
	atomic_int state; /* how this is being used */
	lasr_value value;
};

// TODO: Still not sure this is the best spot for this
// The other candidates are utils.c (where registration currently is)
// or auto-splitter.c which is what depends on the complete list.
// 
// For now, I'm moving everything to one file so it's easier to consider in the
// complete scope.
extern lasr_global * shared_globals;

lasr_global * register_shared_global(char const * key);
int import_shared_global(lasr_global * container, lasr_export * value);

size_t lasr_export_resize(lasr_export * value, size_t len);
void lasr_export_release(lasr_export * value);
lasr_global * lasr_global_release(lasr_global * global);

// void export_shared_globals(lua_State* L, borrowed_data* head);
// ^^ currently static in auto-splitter.c
