#pragma once

#include "settings/definitions.h"
#include <jansson.h>
#include <stdatomic.h>
#include <stdbool.h>

#define LS_INFO_BEHIND_TIME (1 << 0)
#define LS_INFO_LOSING_TIME (1 << 1)
#define LS_INFO_BEST_SPLIT (1 << 2)
#define LS_INFO_BEST_SEGMENT (1 << 3) // Gold split

extern AppConfig cfg;

/**
 * @brief time structure for storing both the real time and game time
 * for any time representation. (i.e. splits, segments, wr etc)
 */
typedef struct ls_time {
    long long real_time; /*!< Real time means the actual real world elapsed time */
    long long game_time; /*!< Game time is the internal time controlled either by the autosplitter, or derived from real_time - load_time */
} ls_time;

/**
 * @brief enum used for selecting comparison methods. This should correspond to the ls_time struct
 * and have one enum value per ls_time member. The enum value is also stored in the splits file
 * as the game's comparison method for determining which time value is authoritative for PBs and the like.
 * Therefore, all enums should always explicitly declare their backed value to prevent drift
 * or split file incompatibilities.
 */
typedef enum ls_time_method {
    LS_REAL_TIME = 0, /*!< LS_REAL_TIME corresponds to ls_time.real_time */
    LS_GAME_TIME = 1, /*!< LS_GAME_TIME corresponds to ls_time.game_time */
} ls_time_method;

typedef struct ls_game {
    char path[PATH_MAX];
    char* title;
    char* theme;
    char* theme_variant;
    ls_time_method comparison_method;
    int attempt_count;
    int finished_count;
    int width;
    int height;
    ls_time world_record;
    long long start_delay;
    char** split_titles;
    char** split_icon_paths; // null if no icons
    bool contains_icons;
    unsigned int split_count;
    ls_time* split_times;
    ls_time* segment_times;
    ls_time* best_splits;
    ls_time* best_segments;
} ls_game;

/**
 * @brief Timer structure for managing game and time.
 * Timer structure, it includes RTA, gametime, loading time, splits, deltas, and other relevant information for tracking the progress of a run.
 */
typedef struct ls_timer {
    bool usingGameTime; /*!< Splitter is using game time instead of real time. Only to be used internally */
    long long gameTime; /*!< The current game time only usable in LASR. Only to be used internally */
    long long realTime; /*!< Real time. Starts when run start and pauses while loading. Only to be used internally */
    int loading; /*!< Currently loading? used for knowing if loadingTime should tick or not. Only to be used internally */
    long long loadingTime; /*!< Time spent loading, used to subtract from real time when trying to get Load-Removed Time. Only to be used internally */
    int started; /*!< Wether the run has started, either by LASR or manually, keeps being set to true after run finished */
    bool running; /*!< Whether the runner is currently running. If this is false and started is true then the run finished. Mainly used to check if some actions are valid to perform (splits, pause, etc) */
    unsigned int curr_split; /*!< Index of the current split, 0 for first split */
    ls_time sum_of_bests; /*!< Sum of best segments */
    ls_time world_record; /*!< World record time */
    ls_time* split_times;
    ls_time* split_deltas;
    ls_time* segment_times;
    ls_time* segment_deltas;
    int* split_info;
    ls_time* best_splits;
    ls_time* best_segments;
    const ls_game* game;
    long long last_tick; // This NEEDS to be here for resetting
    int* attempt_count;
    int* finished_count;
} ls_timer;

typedef int (*timer_hook_func)(const ls_timer* timer);

/**
 * A registry for hook functions for each timer action.
 */
typedef struct _TimerHookRegistry {
    int count; /*!< Current count of functions in the registry */
    int size; /*!< Current size of the registry array */
    timer_hook_func* functions; /*!< Array of fuctions */
    bool active; /*!< Whether the registry is correctly initialized */
} TimerHookRegistry;

extern TimerHookRegistry start_hooks;
extern TimerHookRegistry stop_hooks;
extern TimerHookRegistry split_hooks;
extern TimerHookRegistry reset_hooks;
extern TimerHookRegistry cancel_hooks;
extern TimerHookRegistry skip_hooks;
extern TimerHookRegistry unsplit_hooks;
extern TimerHookRegistry pause_hooks;
extern TimerHookRegistry unpause_hooks;

void init_timer_registries(void);
void free_timer_registries(void);

extern atomic_bool run_started;

ls_time ls_timer_get_time(const ls_timer* timer, bool load_removed);

long long ls_time_value(const char* string);

ls_time ls_time_subtract(ls_time a, ls_time b);

long long ls_segment_value(long long current, long long previous, bool is_first_split);

long long ls_time_get_by_method(ls_time time, ls_time_method method);

bool is_time_valid(long long time);

long long ls_sum_of_bests(const ls_timer* timer, ls_time_method method);

bool ls_time_lte_zero(ls_time time);

void ls_time_clear(ls_time* time);

void ls_time_string(char* string, long long time);

void ls_time_millis_string(char* seconds, char* millis, long long time);

void ls_split_string(char* string, long long time, int compact);

void ls_delta_string(char* string, long long time);

int ls_game_create(ls_game** game_ptr, const char* path, char** error_msg);

void ls_game_update_splits(ls_game* game, const ls_timer* timer);

bool ls_timer_has_gold_split(const ls_timer* timer);

bool ls_timer_has_rainbow_split(const ls_timer* timer);

int ls_game_save(const ls_game* game);

void ls_game_release(ls_game* game);

int ls_timer_create(ls_timer** timer_ptr, ls_game* game);

void ls_timer_release(ls_timer* timer);

int ls_timer_start(ls_timer* timer);

void ls_timer_step(ls_timer* timer);

int ls_timer_split(ls_timer* timer);

int ls_timer_skip(ls_timer* timer);

int ls_timer_unsplit(ls_timer* timer);

void ls_timer_pause(ls_timer* timer);

void ls_timer_unpause(ls_timer* timer);

void ls_timer_stop(ls_timer* timer);

int ls_timer_reset(ls_timer* timer, ls_game* game);

int ls_timer_cancel(ls_timer* timer);

void json_time_get(const json_t* ref, ls_time* time);

void json_time_set(json_t* ref, const ls_time* time);
