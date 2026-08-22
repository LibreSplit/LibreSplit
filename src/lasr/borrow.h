#pragma once

#include <stdatomic.h>
#include <stdbool.h>

#define LASR_STATE_ATOMIC	-1  /* Value is shared via atomic instructions */
#define LASR_STATE_BORROWED	 0  /* Write thread has full control */
#define LASR_STATE_OWNED	 1  /* Read thread has full control */
#define LASR_STATE_NEEDED	 2  /* Write thread requesting full control */

// very rough types skel....needs love
typedef enum {
	LASR_VOID = 0,
	LASR_BOOL = 'b',
	LASR_INT = 'i',
	// LASR_TIME = 't',
	// LASR_DOUBLE = 'd',
	LASR_STRING = 's',
} lasr_type;

/**
 * Lua string type representation (struct extends)
 */
typedef struct _string_data string_data;
struct _string_data {
	size_t size; /* allocated size, not string length */
	char str[];
};

/**
 * Borrowed data base type. Owned by READing thread.
 *
 * (todo docucomment)
 */
typedef struct _owned_data owned_data;
struct _owned_data {
    char const * key; /* consider `name` ?? not sure which is more intuitive */
	owned_data * next; /* linked list next */
	// atomic_bool borrowed; /* true if writing thread has control */
	atomic_int state; /* how this is being used */
	lasr_type const type; /* data type */
	union {
		atomic_int const atomic_data;
		string_data const * dynamic_data;
	};
};

/**
 * Shadow type of owned_data; this form seen by the WRITEing thread.
 *
 * (todo docucomment)
 */
typedef struct _borrowed_data borrowed_data;
struct _borrowed_data {
    char const * const key;
	borrowed_data * const next;
	// atomic_bool borrowed;
	atomic_int state;
	lasr_type type;
	union {
		atomic_int atomic_data;
		string_data * dynamic_data;
	};
};
