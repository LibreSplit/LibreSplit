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

// very rough types skel....needs love
// TODO: Might be better to just use an int and defines...be a little more
// consistent with LUA_TXXX
typedef enum {
	LASR_VOID = 40,
	LASR_BOOL,
	LASR_INT,
	LASR_STRING,
} lasr_type;

/**
 * Lua string type representation (struct extends)
 *
 * TODO: Might nest this within shared_data to be less confusing which is which
 *
 * sizeof(lasr_dynamic_data) === sizeof(size_t)
 * ^^ sizeof(lasr_dynamic_data.bytes) is 0
 */
typedef struct {
	size_t size; /* allocated size, not string length */
	char bytes[];
} lasr_dynamic_data;

/**
 * Union of possible shared data storage formats
 */
// typedef union {
	// atomic_int atomic;
	// string_data * dynamic;
// } shared_data;

// TODO: Consider renaming me??
typedef struct {
	int type; /* data type (LASR_TYPE_XXX) */
	union {
		atomic_int atomic;
		lasr_dynamic_data * dynamic;
	};
} lasr_value;

/* shadow of lasr_value but for the thread-safe use */
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
