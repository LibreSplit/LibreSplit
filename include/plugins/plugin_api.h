#pragma once
#include "lua.h"
#include "timer_structs.h"
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t abi_version_t;
// External declarations
typedef struct _ExternalLASRFunctionRegistry ExternalLASRFunctionRegistry;
typedef struct ls_state ls_state;
typedef struct LSComponent LSComponent;
typedef struct LSComponentOps LSComponentOps;
typedef LSComponent* (*ls_component_new_func)(void);
typedef int (*timer_hook_func)(const ls_state* timer);

/*! \enum event
 *
 *  Describes the events you can register an event hook for.
 */
typedef enum HookableEvent {
    START,
    SPLIT,
    STOP,
    RESET,
    CANCEL,
    SKIP,
    UNSPLIT,
    PAUSE,
    UNPAUSE,
} HookableEvent;
typedef int (*register_lua_func)(const char*, lua_CFunction);
typedef int (*register_event_func)(HookableEvent event, timer_hook_func fn);
typedef int (*register_component_func)(char* name, ls_component_new_func fn);

/**
 * A struct typedef that keeps a snapshot of sorts of the current
 * timer status, to avoid exposing the internal structure completely to
 * plugins.
 *
 * Written at 10.35pm while sick, might be outta whack.
 */
typedef struct ls_state {
    const char* title;
    const char* name;
    const char* category;
    ls_time_method comparison_method;
    const int* attempt_count;
    const int* finished_count;
    const long long start_delay;
    const char** const split_titles;
    const char** const split_icon_paths; // null if no icons
    const bool contains_icons;
    const unsigned int split_count;
    const ls_time* split_times;
    const ls_time* segment_times;
    const ls_time* best_splits;
    const ls_time* best_segments;
    const ls_time* segment_deltas;
    const ls_time sum_of_bests; /*!< Sum of best segments */
    const ls_time world_record; /*!< World record time */
    const unsigned int curr_split; /*!< Index of the current split, 0 for first split */
} ls_state;

typedef struct PlugAPI {
    abi_version_t abi_version;
    register_lua_func register_lua_function;
    register_event_func register_event_hook;
    register_component_func register_component;
} PlugAPI;
