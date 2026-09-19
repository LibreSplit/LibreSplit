#pragma once

#include <linux/limits.h>
#include <lua.h>
#include <stdatomic.h>
#include <stdbool.h>

extern char auto_splitter_file[PATH_MAX];
extern int refresh_rate;
extern bool use_game_time;
extern atomic_bool update_game_time;
extern atomic_llong game_time_value;
extern int maps_cache_cycles;
extern atomic_bool auto_splitter_enabled;
extern atomic_bool auto_splitter_running;
extern atomic_bool call_start;
extern atomic_bool call_split;
extern atomic_bool toggle_loading;
extern atomic_bool call_reset;
extern atomic_bool run_using_game_time_call;
extern atomic_bool run_using_game_time;
extern atomic_bool run_started;
extern atomic_bool run_running;
extern atomic_int lasr_event_requests;
extern bool prev_is_loading;

/** \enum TimerEvent
 *
 * Used to work through the event requests coming from the timer
 * towards the auto splitter
 */
enum TimerEvent {
    TIMER_EVT_START = 1 << 0,
    TIMER_EVT_SPLIT = 1 << 1,
    TIMER_EVT_STOP = 1 << 2,
    TIMER_EVT_RESET = 1 << 3,
    TIMER_EVT_CANCEL = 1 << 4,
    TIMER_EVT_SKIP = 1 << 5,
    TIMER_EVT_UNSPLIT = 1 << 6,
    TIMER_EVT_PAUSE = 1 << 7,
    TIMER_EVT_UNPAUSE = 1 << 8,
};

/**
 * Defines a Lua Auto Splitter Runtime Function.
 */
struct lasr_function {
    char* function_name; /*!< The name of the function in Lua */
    lua_CFunction function_ptr; /*!< C Function to be executed */
} typedef lasr_function;

void check_directories(void);
void run_auto_splitter(void);
