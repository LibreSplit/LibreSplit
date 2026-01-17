#pragma once

#define LS_INFO_BEHIND_TIME (1)
#define LS_INFO_LOSING_TIME (2)
#define LS_INFO_BEST_SPLIT (4)
#define LS_INFO_BEST_SEGMENT (8)

typedef struct ls_game {
    char* path;
    char* title;
    char* theme;
    char* theme_variant;
    int attempt_count;
    int finished_count;
    int width;
    int height;
    long long world_record;
    long long start_delay;
    char** split_titles;
    char** split_icon_paths; // null if no icons
    bool contains_icons;
    int split_count;
    long long* split_times;
    long long* segment_times;
    long long* best_splits;
    long long* best_segments;
} ls_game;

typedef struct ls_timer {
    int started;
    int running;
    int loading;
    int curr_split;
    long long now;
    long long start_time;
    long long time;
    long long sum_of_bests;
    long long world_record;
    long long* split_times;
    long long* split_deltas;
    long long* segment_times;
    long long* segment_deltas;
    int* split_info;
    long long* best_splits;
    long long* best_segments;
    const ls_game* game;
    int* attempt_count;
    int* finished_count;
} ls_timer;

long long ls_time_now(void);

long long ls_time_value(const char* string);

void ls_time_string(char* string, long long time);

void ls_time_millis_string(char* seconds, char* millis, long long time);

void ls_split_string(char* string, long long time, int compact);

void ls_delta_string(char* string, long long time);

int ls_game_create(ls_game** game_ptr, const char* path, char** error_msg);

void ls_game_update_splits(ls_game* game, const ls_timer* timer);

void ls_game_update_bests(const ls_game* game, const ls_timer* timer);

int ls_game_save(const ls_game* game);

void ls_game_release(const ls_game* game);

int ls_timer_create(ls_timer** timer_ptr, ls_game* game);

void ls_timer_release(const ls_timer* timer);

int ls_timer_start(ls_timer* timer);

void ls_timer_step(ls_timer* timer, long long now);

int ls_timer_split(ls_timer* timer);

int ls_timer_skip(ls_timer* timer);

int ls_timer_unsplit(ls_timer* timer);

void ls_timer_stop(ls_timer* timer);

int ls_timer_reset(ls_timer* timer);

int ls_timer_cancel(ls_timer* timer);
