#pragma once

#include "src/lasr/settings.h"
#include "src/settings/definitions.h"
#include <jansson.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define LS_INFO_BEHIND_TIME (1 << 0)
#define LS_INFO_LOSING_TIME (1 << 1)
#define LS_INFO_BEST_SPLIT (1 << 2)
#define LS_INFO_BEST_SEGMENT (1 << 3) // Gold split

#define MAX_TIMESTAMP_LENGTH 256

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

/**
 * @brief The game struct representing a user's loaded splits file.
 * When splits are saved, a snapshot of this struct is created for the
 * save operation. So if this struct is modified, then the equivalent change
 * must be added to the snapshot function in ./src/gui/game.c `create_snapshot`
 * as well as any memory releasing in `ls_game_release`
 */
typedef struct ls_game {
    char path[PATH_MAX];
    char* title;
    char* name;
    char* category;
    char* icon_path;
    char* theme;
    char* theme_variant;
    char* auto_splitter_file;
    char* layout_file;
    ls_time_method comparison_method;
    int attempt_count;
    int finished_count;
    int width;
    int height;
    ls_time world_record;
    long long start_delay;
    uint32_t* split_ids;
    char** split_titles;
    char** split_icon_paths; // null if no icons
    bool contains_icons;
    unsigned int split_count;
    ls_time* split_times;
    ls_time* segment_times;
    ls_time* best_splits;
    ls_time* best_segments;
    atomic_bool has_unsaved_pb;
    atomic_bool has_unsaved_gold;
    atomic_bool has_unsaved_rainbow;
    UserSetting** auto_splitter_settings;
    size_t auto_splitter_settings_count;
} ls_game;

/**
 * @brief Timer structure for managing game and time.
 * Timer structure, it includes RTA, gametime, loading time, splits, deltas, and other relevant information for tracking the progress of a run.
 */
typedef struct ls_timer {
    bool usingGameTime; /*!< Splitter is using game time instead of real time. Only to be used internally */
    long long gameTime; /*!< The current game time only usable in LASR. Only to be used internally */
    long long realTime; /*!< Elapsed real time, including loading while the attempt is running */
    int loading; /*!< Currently loading? used for knowing if loadingTime should tick or not. Only to be used internally */
    long long loadingTime; /*!< Time spent loading, used to subtract from real time when trying to get Load-Removed Time. Only to be used internally */
    int started; /*!< An attempt has started and has not finished or been reset */
    bool running; /*!< Whether elapsed time advances; false for stopped, idle, and finished attempts */
    unsigned int curr_split; /*!< Index of the current split, 0 for first split */
    ls_time sum_of_bests; /*!< Sum of best segments */
    ls_time world_record; /*!< World record time */
    ls_time comparison_time; /*!< Final PB times at the start of this attempt */
    ls_time* split_times;
    ls_time* split_deltas;
    ls_time* segment_times;
    ls_time* segment_deltas;
    int* split_info;
    ls_time* best_splits;
    ls_time* best_segments;
    const ls_game* game;
    long long last_tick; // This NEEDS to be here for resetting
    long long last_loading_time; /*!< Accumulated loading time since last tick */
    int* attempt_count;
    int* finished_count;
    char start_time[64];
} ls_timer;

typedef enum ls_timer_status {
    LS_TIMER_IDLE,
    LS_TIMER_PAUSED,
    LS_TIMER_AHEAD_GAINING,
    LS_TIMER_AHEAD_LOSING,
    LS_TIMER_BEHIND_GAINING,
    LS_TIMER_BEHIND_LOSING,
    LS_TIMER_BEST
} ls_timer_status;

extern atomic_bool run_started;

long long ls_time_now(void);

ls_time ls_timer_get_time(const ls_timer* timer, bool load_removed);

int ls_timer_get_split_pace(const ls_timer* timer, unsigned int index, ls_time_method method);

ls_timer_status ls_timer_get_status(const ls_timer* timer, ls_time_method method);

long long ls_timer_get_segment_time(const ls_timer* timer, ls_time_method method);

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

void ls_game_user_settings_get(UserSetting*** settings, size_t* count);

int ls_game_create(ls_game** game_ptr, const char* path, char** error_msg);

void ls_game_update_splits(ls_game* game, const ls_timer* timer);

bool ls_timer_has_gold_split(const ls_timer* timer);

bool ls_timer_has_rainbow_split(const ls_timer* timer);

bool ls_game_has_achievement(const ls_timer* timer);

bool ls_write_save(json_t* json, const char* path);

int ls_game_save(const ls_game* game);

void ls_game_saved(ls_game* game);

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

void ls_timer_cancel(ls_timer* timer);

void json_time_get(const json_t* ref, ls_time* time);

void json_time_set(json_t* ref, const ls_time* time);

void ls_run_set_time(char* time_buf);
