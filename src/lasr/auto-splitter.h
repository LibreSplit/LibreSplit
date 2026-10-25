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
extern atomic_bool call_reset;
extern atomic_bool run_using_game_time_call;
extern atomic_bool run_using_game_time;
extern atomic_bool run_started;
extern atomic_bool run_running;
extern atomic_bool prev_is_loading;

/**
 * Defines a Lua Auto Splitter Runtime Function.
 */
struct lasr_function {
    char* function_name; /*!< The name of the function in Lua */
    lua_CFunction function_ptr; /*!< C Function to be executed */
} typedef lasr_function;

long long lasr_loading_time(long long* now);
bool lasr_has_loading_request(void);
int lasr_loading_request_pop(void);
bool lasr_reset_loading(void);
void lasr_lock(void);
void lasr_unlock(void);
void lasr_event(const char* name, const char* following);
void init_auto_splitter(void);
void check_directories(void);
void run_auto_splitter(void);
void stop_auto_splitter(void);
