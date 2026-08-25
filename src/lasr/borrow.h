#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#define LASR_STATE_ATOMIC	-1  /* Value is shared via atomic instructions */
#define LASR_STATE_BORROWED	 0  /* Write thread has full control */
#define LASR_STATE_OWNED	 1  /* Read thread has full control */
#define LASR_STATE_NEEDED	 2  /* Write thread requesting full control */

// very rough types skel....needs love
// TODO: Might be better to just use an int and defines...be a little more
// consistent with LUA_TXXX
typedef enum {
	LASR_VOID = 0,
	LASR_BOOL,
	LASR_INT,
	// LASR_TIME,
	// LASR_DOUBLE,
	LASR_STRING,
} lasr_type;

/**
 * Lua string type representation (struct extends)
 *
 * TODO: Might nest this within shared_data to be less confusing which is which
 *
 * sizeof(string_data) === sizeof(size_t)
 * ^^ sizeof(string_data.str) is 0
 */
typedef struct _string_data string_data;
struct _string_data {
	size_t size; /* allocated size, not string length */
	char str[];
};

/**
 * Union of possible shared data storage formats
 */
typedef union {
	atomic_int atomic;
	string_data * dynamic;
} shared_data;

/**
 * Borrowed data base type. Owned by READing thread.
 *
 * (todo docucomment)
 */
typedef struct _owned_data owned_data;
struct _owned_data {
    char const * key; /* consider `name` ?? not sure which is more intuitive */
	owned_data * next; /* linked list next */
	atomic_int state; /* how this is being used */
	lasr_type type; /* data type */
	shared_data data;
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
	atomic_int state;
	lasr_type type;
	shared_data data;
};
