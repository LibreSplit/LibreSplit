#pragma once

#include <linux/limits.h>
#include <lua.h>
#include <stdatomic.h>

extern char auto_splitter_file[PATH_MAX];
extern int refresh_rate;
extern bool use_game_time;
extern atomic_bool update_game_time;
extern atomic_llong game_time_value;
extern int maps_cache_cycles;
extern atomic_bool auto_splitter_enabled;
extern atomic_bool auto_splitter_running;
extern atomic_bool call_start;
extern atomic_bool run_started;
extern atomic_bool run_finished;
extern atomic_bool call_split;
extern atomic_bool toggle_loading;
extern atomic_bool call_reset;
extern bool prev_is_loading;

/**
 * Defines a Lua Auto Splitter Runtime Function.
 */
struct lasr_func_struct {
    char* function_name; /*!< The name of the function in Lua */
    lua_CFunction function_ptr; /*!< C Function to be executed */
} typedef lasr_function;

void check_directories();
void run_auto_splitter();
