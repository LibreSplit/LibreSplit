#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Enumeration of possible 'lasr_global' data exchange states. The active state
 * is an indication of the per-thread data safety.
 *
 * When a race condition is possible, the READing thread may only do so if the
 * data exchange state is ATOMIC or OWNED. Similarly, the WRITEing thread may
 * only modify this value if the exchange state is ATOMIC or BORROWED.
 *
 * NEEDED is a transitional state that shadows OWNED. It is safe for the READ
 * thread to read in this state, but the data is guaranteed to be out of date.
 *
 * The following state transitions are supported:
 * - OWNED    --> BORROWED (invoked by READ thread)
 * - NEEDED   --> BORROWED (invoked by READ thread)
 * - BORROWED --> OWNED    (invoked by WRITE thread)
 * - BORROWED --> ATOMIC   (invoked by WRITE thread)
 * - ATOMIC   --> NEEDED   (invoked by WRITE thread)
 *
 * This creates somewhat of a "publisher / subscriber" relationship, where,
 * unless a data type that can be atomically stored is used, the write thread
 * must have control of the data to modify it, and then it is handed to the
 * read thread. It will only be returned to the write thread once the read
 * thread has consumed the value.
 */
#define LASR_STATE_ATOMIC	-1  /* Value is shared via atomic instructions */
#define LASR_STATE_BORROWED	 0  /* Write thread has full control */
#define LASR_STATE_OWNED	 1  /* Read thread has full control */
#define LASR_STATE_NEEDED	 2  /* Write thread requesting full control */

/**
 * Enumeration of possible `lasr_global` data storage types.
 *
 * These types hint properties that may be assumed about the respective data
 * with respect to their memory allocation and thread safety.
 *
 * INVALID and NIL types mean that the associated data is meaningless. (NIL
 * indicates this is intentional, whereas INVALID arises when incomplete.)
 * ATOMIC means that the data fits into statically-allocated storage where
 * atomic CPU instructions are supported. DYNAMIC indicates that the data is a
 * heap-allocated array of a known length.
 *
 * Only DYNAMIC data types require a strict data exchange handoff (usage of the
 * OWNED / BORROWED states.)
 *
 * Implementation note:
 * I was originally planning more granular types such as floats or timestamps,
 * but this became the most effective implementation. Thus, it is (currently)
 * the responsibility of the component that needs the value to assume the data
 * format and perform conversions as necessary.
 */
#define LASR_TYPE_INVALID	 0  /* Corresponding data cannot be resolved and should be ignored */
#define LASR_TYPE_NIL		 1  /* There exists no data (null, none, nil, etc.) */
#define LASR_TYPE_ATOMIC	 2  /* "Simple" data that can be read with atomic instructions */
#define LASR_TYPE_DYNAMIC	 3  /* "Array" data that requires stricter thread safety */

/**
 * Type for variable-length data of a known length for use with lua shared
 * global exports. Formats the payload with its length as a prefix into an
 * extensible struct to avoid nested memory allocations.
 */
typedef struct {
	size_t len;   /*!> length of payload, excludes a NUL byte */
	char bytes[]; /*!> Array pointer to payload (struct extends) */
} lasr_dynamic_data;

/**
 * Type that unifies "fixed" or "dynamic" shared export data as values returned
 * from the lua context are only known at runtime. Accesses to this struct
 * should be contained within thread-conscious routines following the
 * LASR_STATE state machine.
 */
typedef struct {
	int const type;        /*!> data type (LASR_TYPE_XXX) */
	union {
		atomic_int atomic; /*!> accessor for fixed-length data */
		lasr_dynamic_data const * const dynamic; /*!> accessor for variable-length data */
	};
} lasr_value;

/**
 * Shadow type of 'lasr_value' that can be used in a single-threaded context.
 * Notably, local copies of exported values should use this as a destination
 * buffer during the exchange routine.
 */
typedef struct {
	int type;      /*!> data type (LASR_TYPE_XXX) */
	union {
		int fixed; /*!> accessor for fixed-length data */
		lasr_dynamic_data * dynamic; /*!> accessor for variable-length data */
	};
} lasr_export;

/**
 * Container type for exported lua globals. Maintains the variable name, the
 * data exchange state, and the value itself. The memory allocation of this
 * value should be managed by the thread that WRITEs to this container.
 *
 * Also included is a reference to a lasr_global so that these types may be
 * chained as a linked-list. This value should ONLY be referenced by the WRITE
 * thread. Do not use this reference for memory release.
 *
 * At all times, the data exchange state and the value's type should be in sync.
 * Any operation that modifies these must ensure this is the case.
 */
typedef struct _lasr_global lasr_global;
struct _lasr_global {
    char const * key;   /*!> name of tracked lua variable */
	lasr_global * next; /*!> next tracked value in sequence */
	atomic_int state;   /*!> data exchange state */
	lasr_value value;   /*!> exported value */
};

lasr_global * lasr_global_create(char const * key);
void export_atomic_global(lasr_global * container, int const value, int const type);
void export_dynamic_global(lasr_global * container, char const * const value, size_t const len);
int import_shared_global(lasr_global * container, lasr_export * target);

size_t lasr_export_resize(lasr_export * value, size_t len);
void lasr_global_release(lasr_global * global);
