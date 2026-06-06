/** \file timer.c
 *
 * Implementation of the timer
 */
#include "timer.h"
#include "gui/dialogs.h"
#include "logging.h"
#include "settings/utils.h"

#include "lasr/auto-splitter.h"

#include <jansson.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Returns the current time, taken from a monotonic clock
 * (a clock that is not affected by leap seconds or daylight savings).
 *
 * @return The current time, in milliseconds
 */
static long long ls_time_now(void)
{
    struct timespec timespec;
    clock_gettime(CLOCK_MONOTONIC, &timespec);
    return timespec.tv_sec * 1000000LL + timespec.tv_nsec / 1000;
}

/**
 * Gets the timer current time, either game time or real time depending on the timer state.
 *
 * @param timer The timer instance
 * @param load_removed Whether to subtract load_removed from RTA time
 * @return The current time
 */
inline long long ls_timer_get_time(const ls_timer* timer, bool load_removed)
{
    if (timer->usingGameTime) {
        return timer->gameTime;
    }

    if (load_removed) {
        return timer->realTime - timer->loadingTime;
    }

    return timer->realTime;
}

/**
 * Converts a time string into milliseconds
 *
 * Takes a HH:MM:SS.mmmmmm formatted time string and converts it into
 * milliseconds.
 *
 * @param string The time string to convert, in HH:MM:SS.mmmmmm format
 * @return The time string converted to milliseconds
 */
long long ls_time_value(const char* string)
{
    char seconds_part[256];
    double subseconds_part = 0.;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int sign = 1;
    if (!string || !strlen(string)) {
        return 0;
    }

    // Split at the decimal point manually
    const char* dot_pos = strchr(string, '.');
    if (dot_pos) {
        strncpy(seconds_part, string, dot_pos - string);
        seconds_part[dot_pos - string] = '\0';

        // Manually parse the fractional part to avoid locale issues
        const char* frac_part = dot_pos + 1;
        subseconds_part = 0.0;
        double multiplier = 0.1;

        for (char* p = (char*)frac_part; *p && *p >= '0' && *p <= '9'; p++) {
            subseconds_part += (*p - '0') * multiplier;
            multiplier *= 0.1;
        }
    } else {
        strcpy(seconds_part, string);
        subseconds_part = 0.0;
    }

    if (seconds_part[0] == '-') {
        sign = -1;
        memmove(seconds_part, seconds_part + 1, strlen(seconds_part));
    }
    switch (sscanf(seconds_part, "%d:%d:%d", &hours, &minutes, &seconds)) {
        case 2:
            seconds = minutes;
            minutes = hours;
            hours = 0;
            break;
        case 1:
            seconds = hours;
            minutes = 0;
            hours = 0;
            break;
    }

    return sign * ((hours * 60 * 60 + minutes * 60 + seconds) * 1000000LL + (long long)(subseconds_part * 1000000.));
}

/**
 * Converts a time in milliseconds to a formatted string.
 *
 * Takes a time in milliseconds and converts it into a human-readable format
 * copying it via side-effect into the first and second argument, a bit
 * like strcpy would do.
 *
 * @param string The destination where to copy the formatted string to.
 * @param millis The destination where to copy the subseconds part string to.
 * @param time The time to convert
 * @param serialized Show all 6 decimal places, if set to zero will only show 2
 * @param delta Show the time as a delta, when negative
 * @param compact Defines whether to use the "extended" or "compact" formatting
 */
static void ls_time_string_format(char* string,
    char* millis,
    long long time,
    int serialized,
    int delta,
    int compact)
{
    int hours, minutes, seconds;
    char dot_subsecs[256];
    const char* sign = "";

    // Check time is not 0 or maxed out, otherwise -
    if (time == LLONG_MAX) {
        sprintf(string, "-");
        return;
    }

    if (time < 0) {
        time = -time;
        sign = "-";
    } else if (delta) {
        sign = "+";
    }
    hours = time / (1000000LL * 60 * 60);
    minutes = (time / (1000000LL * 60)) % 60;
    seconds = (time / 1000000LL) % 60;
    sprintf(dot_subsecs, ".%06lld", time % 1000000LL);
    int display_decimals = cfg.libresplit.decimals.value.i;
    if (!serialized) {
        int subsec_idx = 0;
        if (display_decimals <= 0) {
            subsec_idx = 0;
        } else if (display_decimals > 6) {
            subsec_idx = 7;
        } else {
            subsec_idx = display_decimals + 1;
        }
        /* Show only a dot and x decimal places instead of all 6 */
        memset(&dot_subsecs[subsec_idx], '\0', sizeof(dot_subsecs) - subsec_idx);
    }
    if (millis) {
        strcpy(millis, &dot_subsecs[1]);
        dot_subsecs[0] = '\0';
    }
    if (hours) {
        if (compact) {
            sprintf(string, "%s%d:%02d:%02d", sign, hours, minutes, seconds);
        } else {
            sprintf(string, "%s%d:%02d:%02d%s",
                sign, hours, minutes, seconds, dot_subsecs);
        }
    } else if (minutes) {
        if (compact) {
            sprintf(string, "%s%d:%02d", sign, minutes, seconds);
        } else {
            sprintf(string, "%s%d:%02d%s",
                sign, minutes, seconds, dot_subsecs);
        }
    } else {
        sprintf(string, "%s%d%s", sign, seconds, dot_subsecs);
    }
}

static void ls_time_string_serialized(char* string,
    long long time)
{
    ls_time_string_format(string, NULL, time, 1, 0, 0);
}

void ls_time_string(char* string, long long time)
{
    ls_time_string_format(string, NULL, time, 0, 0, 0);
}

void ls_time_millis_string(char* seconds, char* millis, long long time)
{
    ls_time_string_format(seconds, millis, time, 0, 0, 0);
}

void ls_split_string(char* string, long long time, int compact)
{
    ls_time_string_format(string, NULL, time, 0, 0, compact);
}

void ls_delta_string(char* string, long long time)
{
    ls_time_string_format(string, NULL, time, 0, 1, 1);
}

/**
 * Frees the memory allocated for a game struct and sets all its pointers to NULL.
 *
 * @param game
 */
void ls_game_release(ls_game* game)
{
    if (game->title) {
        free(game->title);
        game->title = 0;
    }
    if (game->theme) {
        free(game->theme);
        game->theme = 0;
    }
    if (game->theme_variant) {
        free(game->theme_variant);
        game->theme_variant = 0;
    }
    if (game->split_titles) {
        for (unsigned int i = 0; i < game->split_count; ++i) {
            if (game->split_titles[i]) {
                free(game->split_titles[i]);
                game->split_titles[i] = 0;
            }
        }
        free(game->split_titles);
        game->split_titles = 0;
    }
    if (game->split_times) {
        free(game->split_times);
        game->split_times = 0;
    }
    if (game->split_icon_paths) {
        free(game->split_icon_paths);
    }
    if (game->segment_times) {
        free(game->segment_times);
        game->segment_times = 0;
    }
    if (game->best_splits) {
        free(game->best_splits);
        game->best_splits = 0;
    }
    if (game->best_segments) {
        free(game->best_segments);
        game->best_segments = 0;
    }

    free(game);
}

int ls_game_create(ls_game** game_ptr, const char* path, char** error_msg)
{
    int error = 0;
    json_t* json = 0;
    json_t* ref;
    json_error_t json_error;
    // allocate game
    ls_game* game = calloc(1, sizeof(ls_game));
    if (!game) {
        error = 1;
        goto game_create_error;
    }
    // copy path to file
    strncpy(game->path, path, PATH_MAX - 1);
    game->path[PATH_MAX - 1] = '\0';
    // load json
    json = json_load_file(game->path, 0, &json_error);
    if (!json) {
        error = 1;
        size_t msg_len = snprintf(NULL, 0, "%s (%d:%d)", json_error.text, json_error.line, json_error.column);
        *error_msg = calloc(msg_len + 1, sizeof(char));
        if (*error_msg == NULL) {
            LOG_ERR("Cannot allocate memory for error message");
            error = 1;
            goto game_create_error;
        }
        sprintf(*error_msg, "%s (%d:%d)", json_error.text, json_error.line, json_error.column);
        goto game_create_error;
    }
    // copy title
    ref = json_object_get(json, "title");
    if (ref) {
        game->title = strdup(json_string_value(ref));
        if (!game->title) {
            error = 1;
            goto game_create_error;
        }
    }
    // copy theme
    ref = json_object_get(json, "theme");
    if (ref) {
        game->theme = strdup(json_string_value(ref));
        if (!game->theme) {
            error = 1;
            goto game_create_error;
        }
    }
    // copy theme variant
    ref = json_object_get(json, "theme_variant");
    if (ref) {
        game->theme_variant = strdup(json_string_value(ref));
        if (!game->theme_variant) {
            error = 1;
            goto game_create_error;
        }
    }
    // get attempt count
    ref = json_object_get(json, "attempt_count");
    if (ref) {
        game->attempt_count = json_integer_value(ref);
    }
    // get finished count
    ref = json_object_get(json, "finished_count");
    if (ref) {
        game->finished_count = json_integer_value(ref);
    }
    // get width
    ref = json_object_get(json, "width");
    if (ref) {
        game->width = json_integer_value(ref);
    }
    // get height
    ref = json_object_get(json, "height");
    if (ref) {
        game->height = json_integer_value(ref);
    }
    // get delay
    ref = json_object_get(json, "start_delay");
    if (ref) {
        game->start_delay = ls_time_value(
            json_string_value(ref));
    }
    // get wr
    ref = json_object_get(json, "world_record");
    if (ref) {
        game->world_record = ls_time_value(
            json_string_value(ref));
    }
    // get splits
    ref = json_object_get(json, "splits");
    if (ref) {
        game->split_count = json_array_size(ref);

        int split_count = game->split_count + 1; // +1 for the final split to end cursor on

        // allocate titles
        game->split_titles = calloc(split_count, sizeof(char*));
        if (!game->split_titles) {
            error = 1;
            goto game_create_error;
        }
        // allocate splits
        game->split_times = calloc(split_count, sizeof(long long));
        if (!game->split_times) {
            error = 1;
            goto game_create_error;
        }
        game->split_icon_paths = calloc(split_count, sizeof(char*));
        if (!game->split_icon_paths) {
            error = 1;
            goto game_create_error;
        }
        game->segment_times = calloc(split_count, sizeof(long long));
        if (!game->segment_times) {
            error = 1;
            goto game_create_error;
        }
        game->best_splits = calloc(split_count, sizeof(long long));
        if (!game->best_splits) {
            error = 1;
            goto game_create_error;
        }
        game->best_segments = calloc(split_count, sizeof(long long));
        if (!game->best_segments) {
            error = 1;
            goto game_create_error;
        }
        game->contains_icons = false;
        // copy splits
        for (unsigned int i = 0; i < game->split_count; ++i) {
            json_t* split;
            json_t* split_ref;
            split = json_array_get(ref, i);
            split_ref = json_object_get(split, "title");
            if (split_ref) {
                game->split_titles[i] = strdup(
                    json_string_value(split_ref));
                if (!game->split_titles[i]) {
                    error = 1;
                    goto game_create_error;
                }
            }

            split_ref = json_object_get(split, "icon");
            if (split_ref) {
                game->split_icon_paths[i] = strdup(json_string_value(split_ref));
                if (!game->split_icon_paths[i]) {
                    error = 1;
                    goto game_create_error;
                }
                game->contains_icons = true;
            }

            split_ref = json_object_get(split, "time");
            if (split_ref) {
                game->split_times[i] = ls_time_value(
                    json_string_value(split_ref));
            }

            // Check whether the split time is 0, if it is set it to max value
            if (game->split_times[i] == 0) {
                game->split_times[i] = LLONG_MAX;
            }
            if (i && game->split_times[i] && game->split_times[i - 1]) {
                game->segment_times[i] = game->split_times[i] - game->split_times[i - 1];
            } else if (!i && game->split_times[0]) {
                game->segment_times[0] = game->split_times[0];
            }

            if (game->best_splits[i] == 0) {
                game->best_splits[i] = LLONG_MAX;
            }
            split_ref = json_object_get(split, "best_time");
            if (split_ref) {
                game->best_splits[i] = ls_time_value(
                    json_string_value(split_ref));
            } else if (game->split_times[i]) {
                game->best_splits[i] = game->split_times[i];
            }

            if (game->best_segments[i] == 0) {
                game->best_segments[i] = LLONG_MAX;
            }
            split_ref = json_object_get(split, "best_segment");
            if (split_ref) {
                game->best_segments[i] = ls_time_value(
                    json_string_value(split_ref));
            } else if (game->segment_times[i]) {
                game->best_segments[i] = game->segment_times[i];
            }
        }
    }
game_create_error:
    if (json) {
        json_decref(json);
    }

    if (error) {
        if (game) {
            ls_game_release(game);
            game = 0;
        }
        return error;
    }

    // Free all of the old game's data before replacing the pointer
    if (*game_ptr) {
        ls_game_release(*game_ptr);
        *game_ptr = 0;
    }
    *game_ptr = game;

    return 0;
}

/**
 * Update the splits of a game based on the current timer.
 *
 * @param game The game whose splits are to be updated.
 * @param timer The timer instance
 */
void ls_game_update_splits(ls_game* game, const ls_timer* timer)
{
    if (timer->curr_split) {
        int size;
        if (timer->split_times[game->split_count - 1]
            && timer->split_times[game->split_count - 1]
                < game->world_record) {
            game->world_record = timer->split_times[game->split_count - 1];
        }
        size = timer->curr_split * sizeof(long long);
        if (timer->split_times[game->split_count - 1]
            < game->split_times[game->split_count - 1]) {
            memcpy(game->split_times, timer->split_times, size);
        }
        memcpy(game->segment_times, timer->segment_times, size);
        for (unsigned int i = 0; i < game->split_count; ++i) {
            if (timer->split_times[i] < game->best_splits[i]) {
                game->best_splits[i] = timer->split_times[i];
            }
            if (timer->segment_times[i] < game->best_segments[i]) {
                game->best_segments[i] = timer->segment_times[i];
            }
        }
    }
}

void ls_game_update_bests(const ls_game* game,
    const ls_timer* timer)
{
    if (timer->curr_split) {
        int size;
        size = timer->curr_split * sizeof(long long);
        memcpy(game->best_splits, timer->best_splits, size);
        memcpy(game->best_segments, timer->best_segments, size);
    }
}

bool ls_timer_has_gold_split(const ls_timer* timer)
{
    if (!timer || !timer->split_info)
        return false;

    // Only consider splits that happened this run
    const int committed = timer->curr_split;
    for (int i = 0; i < committed; i++) {
        if (timer->split_info[i] & LS_INFO_BEST_SEGMENT) {
            return true;
        }
    }
    return false;
}

int ls_game_save(const ls_game* game)
{
    int error = 0;
    char str[256];
    json_t* json = json_object();
    json_t* splits = json_array();
    if (game->title) {
        json_object_set_new(json, "title", json_string(game->title));
    }
    if (game->attempt_count) {
        json_object_set_new(json, "attempt_count",
            json_integer(game->attempt_count));
    }
    if (game->finished_count) {
        json_object_set_new(json, "finished_count",
            json_integer(game->finished_count));
    }
    if (game->world_record) {
        ls_time_string_serialized(str, game->world_record);
        json_object_set_new(json, "world_record", json_string(str));
    }
    if (game->start_delay) {
        ls_time_string_serialized(str, game->start_delay);
        json_object_set_new(json, "start_delay", json_string(str));
    }
    for (unsigned int i = 0; i < game->split_count; ++i) {
        json_t* split = json_object();
        json_object_set_new(split, "title", json_string(game->split_titles[i]));
        json_object_set_new(split, "icon", json_string(game->split_icon_paths[i]));

        // Only save the split if it's above 0. Otherwise it's impossible to beat 0
        if (game->split_times[i] > 0 && game->split_times[i] < LLONG_MAX) {
            ls_time_string_serialized(str, game->split_times[i]);
            json_object_set_new(split, "time", json_string(str));
        }
        if (game->best_splits[i] > 0 && game->best_splits[i] < LLONG_MAX) {
            ls_time_string_serialized(str, game->best_splits[i]);
            json_object_set_new(split, "best_time", json_string(str));
        }
        if (game->best_segments[i] > 0 && game->best_segments[i] < LLONG_MAX) {
            ls_time_string_serialized(str, game->best_segments[i]);
            json_object_set_new(split, "best_segment", json_string(str));
        }
        json_array_append_new(splits, split);
    }
    json_object_set_new(json, "splits", splits);
    if (game->theme) {
        json_object_set_new(json, "theme", json_string(game->theme));
    }
    if (game->theme_variant) {
        json_object_set_new(json, "theme_variant",
            json_string(game->theme_variant));
    }
    if (game->width) {
        json_object_set_new(json, "width", json_integer(game->width));
    }
    if (game->height) {
        json_object_set_new(json, "height", json_integer(game->height));
    }
    const int json_dump_result = json_dump_file(json, game->path, JSON_PRESERVE_ORDER | JSON_INDENT(2));
    if (json_dump_result) {
        printf("Error dumping JSON:\n%s\n", json_dumps(json, JSON_PRESERVE_ORDER | JSON_INDENT(2)));
        printf("Error: '%d'\n", json_dump_result);
        printf("Path: %s\n", game->path);
        error = 1;
    }
    json_decref(json);
    return error;
}

int ls_run_save(ls_timer* timer, const char* reason)
{
    if (ls_timer_get_time(timer, true) == 0)
        return 0;

    int error = 0;
    char final_time_str[128];
    ls_time_string_serialized(final_time_str, ls_timer_get_time(timer, true));

    // Root JSON Object
    json_t* json = json_object();

    // Basic Run Info
    if (timer->game->title) {
        json_object_set_new(json, "title", json_string(timer->game->title));
    }
    if (timer->game->attempt_count) {
        json_object_set_new(json, "attempt_count", json_integer(timer->game->attempt_count));
    }
    if (timer->game->finished_count) {
        json_object_set_new(json, "finished_count", json_integer(timer->game->finished_count));
    }
    json_object_set_new(json, "final_time", json_string(final_time_str));
    json_object_set_new(json, "reason", json_string(reason));

    // Splits Array
    json_t* splits = json_array();

    for (unsigned int i = 0; i < timer->game->split_count; i++) {
        json_t* split = json_object();

        // Title
        json_object_set_new(split, "title", json_string(timer->game->split_titles[i]));

        // Time
        if (i < timer->curr_split) {
            // Check if time > 0, avoids saving time on skipped splits
            if (timer->split_times[i] > 0 && timer->split_times[i] < LLONG_MAX) {
                char split_time_str[128];
                ls_time_string_serialized(split_time_str, timer->split_times[i]);
                json_object_set_new(split, "time", json_string(split_time_str));
                // Check if segment time > 0, avoids saving segment time AFTER skipped split
                if (timer->segment_times[i] > 0 && timer->segment_times[i] < LLONG_MAX) {
                    char segment_time_str[128];
                    ls_time_string_serialized(segment_time_str, timer->segment_times[i]);
                    json_object_set_new(split, "segment", json_string(segment_time_str));
                } else {
                    json_object_set_new(split, "segment", json_null());
                }
            } else {
                json_object_set_new(split, "time", json_null());
                json_object_set_new(split, "segment", json_null());
            }
        }
        json_array_append_new(splits, split);
    }

    json_object_set_new(json, "splits", splits);

    char path[PATH_MAX];
    get_libresplit_folder_path(path);
    strncat(path, "/runs", sizeof(path) - strlen(path) - 1);

    time_t rawtime;
    struct tm* timeinfo;
    char time_buf[64];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", timeinfo);

    char filename[PATH_MAX];
    int ret = snprintf(filename, sizeof(filename), "%s/run_%s.json", path, time_buf);
    if (ret < 0 || (size_t)ret >= sizeof(filename)) {
        printf("Error creating run filename. The path may be too long, aborting save.\n");
        return 1;
    }

    const int json_dump_result = json_dump_file(json, filename, JSON_PRESERVE_ORDER | JSON_INDENT(2));
    if (json_dump_result) {
        printf("Error dumping JSON:\n%s\n", json_dumps(json, JSON_PRESERVE_ORDER | JSON_INDENT(2)));
        printf("Error: '%d'\n", json_dump_result);
        printf("Path: %s\n", filename);
        error = 1;
    }

    json_decref(json);
    return error;
}

/**
 * Frees all the timer information, but not itself
 *
 * @param timer The timer instance
 */
void ls_timer_release(ls_timer* timer)
{
    if (timer->split_times) {
        free(timer->split_times);
    }
    if (timer->split_deltas) {
        free(timer->split_deltas);
    }
    if (timer->segment_times) {
        free(timer->segment_times);
    }
    if (timer->segment_deltas) {
        free(timer->segment_deltas);
    }
    if (timer->split_info) {
        free(timer->split_info);
    }
    if (timer->best_splits) {
        free(timer->best_splits);
    }
    if (timer->best_segments) {
        free(timer->best_segments);
    }

    free(timer);
}

/**
 * Resets the whole timer back to 0, ready for a new run
 *
 * @param timer The timer instance
 */
static void reset_timer(ls_timer* timer)
{
    timer->started = 0;
    atomic_store(&run_started, false);
    timer->running = 0;
    atomic_store(&run_running, false);
    timer->curr_split = 0;
    timer->realTime = -timer->game->start_delay; // Start delay only applies to real time only
    timer->gameTime = 0;
    timer->usingGameTime = false;
    timer->loading = false;
    timer->loadingTime = 0;
    timer->last_tick = 0;
    int size = timer->game->split_count * sizeof(long long);
    memcpy(timer->split_times, timer->game->split_times, size);
    memset(timer->split_deltas, 0, size);
    memcpy(timer->segment_times, timer->game->segment_times, size);
    memset(timer->segment_deltas, 0, size);
    memcpy(timer->best_splits, timer->game->best_splits, size);
    memcpy(timer->best_segments, timer->game->best_segments, size);
    size = timer->game->split_count * sizeof(int);
    memset(timer->split_info, 0, size);
    timer->sum_of_bests = 0;
    for (unsigned int i = 0; i < timer->game->split_count; ++i) {
        // Check no segments are erroring with LLONG_MAX
        if (timer->best_segments[i] && timer->best_segments[i] < LLONG_MAX) {
            timer->sum_of_bests += timer->best_segments[i];
        } else if (timer->game->best_segments[i] && timer->game->best_segments[i] < LLONG_MAX) {
            timer->sum_of_bests += timer->game->best_segments[i];
        } else {
            timer->sum_of_bests = 0;
            break;
        }
    }
}

/**
 * Creates a timer instance linked to a game instance, allocating necessary memory
 *
 * @param timer_ptr Apointer to where the allocated timer instance should be stored
 * @param game The game instance to link the timer to
 * @return Whether the timer creation had an error or not
 */
int ls_timer_create(ls_timer** timer_ptr, ls_game* game)
{
    int error = 0;
    ls_timer* timer;
    // allocate timer
    timer = calloc(1, sizeof(ls_timer));
    if (!timer) {
        error = 1;
        goto timer_create_error;
    }
    timer->game = game;
    timer->attempt_count = &game->attempt_count;
    timer->finished_count = &game->finished_count;
    // alloc splits
    int split_count = timer->game->split_count + 1; // +1 for the last invisible "split" that exists to signify no split

    timer->split_times = calloc(split_count, sizeof(long long));
    if (!timer->split_times) {
        error = 1;
        goto timer_create_error;
    }
    timer->split_deltas = calloc(split_count, sizeof(long long));
    if (!timer->split_deltas) {
        error = 1;
        goto timer_create_error;
    }
    timer->segment_times = calloc(split_count, sizeof(long long));
    if (!timer->segment_times) {
        error = 1;
        goto timer_create_error;
    }
    timer->segment_deltas = calloc(split_count, sizeof(long long));
    if (!timer->segment_deltas) {
        error = 1;
        goto timer_create_error;
    }
    timer->best_splits = calloc(split_count, sizeof(long long));
    if (!timer->best_splits) {
        error = 1;
        goto timer_create_error;
    }
    timer->best_segments = calloc(split_count, sizeof(long long));
    if (!timer->best_segments) {
        error = 1;
        goto timer_create_error;
    }
    timer->split_info = calloc(split_count, sizeof(int));
    if (!timer->split_info) {
        error = 1;
        goto timer_create_error;
    }
    reset_timer(timer);
timer_create_error:
    if (error) {
        if (timer) {
            ls_timer_release(timer);
            timer = 0;
        }
        return error;
    }

    // Free old timer before replacing the pointer
    if (*timer_ptr) {
        ls_timer_release(*timer_ptr);
        *timer_ptr = 0;
    }
    *timer_ptr = timer;
    return 0;
}

/**
 * Executes a timer step, calculating deltas, times, and split infos
 *
 * @param timer The timer instance
 */
void ls_timer_step(ls_timer* timer)
{
    long long now = ls_time_now();
    if (timer->running) {
        long long delta = timer->last_tick ? now - timer->last_tick : 0;
        timer->realTime += delta; // Accumulate the elapsed time
        if (timer->loading) {
            timer->loadingTime += delta; // Accumulate loading time if currently loading
        }
        if (timer->curr_split < timer->game->split_count) {
            timer->split_times[timer->curr_split] = timer->usingGameTime ? timer->gameTime : timer->realTime - timer->loadingTime;
            // calc delta and check it's not an error of LLONG_MAX
            if (timer->game->split_times[timer->curr_split] && timer->game->split_times[timer->curr_split] < LLONG_MAX) {
                timer->split_deltas[timer->curr_split] = timer->split_times[timer->curr_split]
                    - timer->game->split_times[timer->curr_split];
            }
            // check for behind time
            if (timer->split_deltas[timer->curr_split] > 0) {
                timer->split_info[timer->curr_split] |= LS_INFO_BEHIND_TIME;
            } else {
                timer->split_info[timer->curr_split] &= ~LS_INFO_BEHIND_TIME;
            }
            if (!timer->curr_split || timer->split_times[timer->curr_split - 1]) {
                // calc segment time and delta
                timer->segment_times[timer->curr_split] = timer->split_times[timer->curr_split];
                if (timer->curr_split) {
                    timer->segment_times[timer->curr_split] -= timer->split_times[timer->curr_split - 1];
                }
                // For previous segment in footer
                if (timer->game->segment_times[timer->curr_split] && timer->game->segment_times[timer->curr_split] < LLONG_MAX) {
                    timer->segment_deltas[timer->curr_split] = timer->segment_times[timer->curr_split]
                        - timer->game->segment_times[timer->curr_split];
                }
            }
            // check for losing time
            if (timer->curr_split) {
                if (timer->split_deltas[timer->curr_split]
                    > timer->split_deltas[timer->curr_split - 1]) {
                    timer->split_info[timer->curr_split]
                        |= LS_INFO_LOSING_TIME;
                } else {
                    timer->split_info[timer->curr_split]
                        &= ~LS_INFO_LOSING_TIME;
                }
            } else if (timer->split_deltas[timer->curr_split] > 0) {
                timer->split_info[timer->curr_split]
                    |= LS_INFO_LOSING_TIME;
            } else {
                timer->split_info[timer->curr_split]
                    &= ~LS_INFO_LOSING_TIME;
            }
        }
    }
    timer->last_tick = now; // Update the start time for the next iteration
}

/**
 * Starts the timer, setting it to running and incrementing attempt count if not already started
 *
 * @param timer The timer instance
 * @return Whether the timer is now running
 */
int ls_timer_start(ls_timer* timer)
{
    // TODO: Allow starting when split_count is 0 for splitless runs, other stuff has to change for this to work (components, timer logic, etc)
    if (timer->curr_split < timer->game->split_count) {
        if (!timer->started) {
            ++*timer->attempt_count;
            timer->started = 1;
            atomic_store(&run_started, true);
        }
        timer->running = true;
        atomic_store(&run_running, true);
    }
    return timer->running;
}

/**
 * Performs a split
 *
 * @param timer The timer instance
 * @return The current split index after splitting, 0 if no split happened
 */
int ls_timer_split(ls_timer* timer)
{
    if (ls_timer_get_time(timer, true) <= 0) {
        return 0;
    }

    if (timer->curr_split >= timer->game->split_count) {
        return 0;
    }

    // check for best split and segment
    if (!timer->best_splits[timer->curr_split]
        || timer->split_times[timer->curr_split]
            < timer->best_splits[timer->curr_split]) {
        timer->best_splits[timer->curr_split] = timer->split_times[timer->curr_split];
        timer->split_info[timer->curr_split]
            |= LS_INFO_BEST_SPLIT;
    }
    if (!timer->best_segments[timer->curr_split]
        || timer->segment_times[timer->curr_split]
            < timer->best_segments[timer->curr_split]) {
        timer->best_segments[timer->curr_split] = timer->segment_times[timer->curr_split];
        timer->split_info[timer->curr_split]
            |= LS_INFO_BEST_SEGMENT;
    }
    // update sum of bests
    timer->sum_of_bests = 0;
    for (unsigned int i = 0; i < timer->game->split_count; ++i) {
        // Check if any best segment is missing/LLONG_MAX
        if (timer->best_segments[i] && timer->best_segments[i] < LLONG_MAX) {
            timer->sum_of_bests += timer->best_segments[i];
        } else if (timer->game->best_segments[i] && timer->game->best_segments[i] < LLONG_MAX) {
            timer->sum_of_bests += timer->game->best_segments[i];
        } else {
            timer->sum_of_bests = 0;
            break;
        }
    }

    ++timer->curr_split;
    // stop timer if last split
    if (timer->curr_split == timer->game->split_count) {
        // Increment finished_count
        ++*timer->finished_count;
        ls_timer_stop(timer);
        ls_game_update_splits((ls_game*)timer->game, timer);
        if (cfg.libresplit.save_run_history.value.b) {
            ls_run_save(timer, "FINISHED");
        }
    }
    return timer->curr_split;
}

/**
 * Skips a split, moving the timer forward one split and setting the split and segment times and deltas to 0
 *
 * @param timer The timer instance
 * @return The current split index after skipping, 0 if no skip happened
 */
int ls_timer_skip(ls_timer* timer)
{
    if (ls_timer_get_time(timer, false) <= 0)
        return 0;

    if (timer->curr_split + 1 == timer->game->split_count) {
        // This is the last split, do a normal split instead of skipping
        return ls_timer_split(timer);
    }

    if (timer->curr_split >= timer->game->split_count) {
        return 0;
    }

    timer->split_times[timer->curr_split] = 0;
    timer->split_deltas[timer->curr_split] = 0;
    timer->split_info[timer->curr_split] = 0;
    timer->segment_times[timer->curr_split] = 0;
    timer->segment_deltas[timer->curr_split] = 0;
    return ++timer->curr_split;
}

/**
 * Unsplits the last split, moving the timer back one split and resetting the split and segment times and deltas to the game times
 *
 * @param timer The timer instance
 * @return The current split index after unsplitting, the same or 0 if no unsplit happened
 */
int ls_timer_unsplit(ls_timer* timer)
{
    if (timer->curr_split == 0) {
        return 0;
    }

    unsigned int curr = --timer->curr_split;
    for (unsigned int i = curr; i < timer->game->split_count; ++i) {
        timer->split_times[i] = timer->game->split_times[i];
        timer->split_deltas[i] = 0;
        timer->split_info[i] = 0;
        timer->segment_times[i] = timer->game->segment_times[i];
        timer->segment_deltas[i] = 0;
    }
    if (timer->curr_split + 1 == timer->game->split_count) {
        timer->running = true;
        atomic_store(&run_running, true);
    }
    return timer->curr_split;
}

/**
 * Marks the timer as loading, incrementing loading time in step until unpaused
 *
 * @param timer The timer instance
 */
void ls_timer_pause(ls_timer* timer)
{
    timer->loading = 1;
}

/**
 * Marks the timer as not loading, not incrementing loading time
 *
 * @param timer The timer instance
 */
void ls_timer_unpause(ls_timer* timer)
{
    timer->loading = 0;
}

/**
 * Stops the timer from ticking
 *
 * @param timer The timer instance
 */
void ls_timer_stop(ls_timer* timer)
{
    timer->running = false;
    atomic_store(&run_running, false);
}

/**
 * Resets all the timer and splits back to 0
 *
 * Also saves run
 *
 * @param timer The timer instance
 * @return Whether the reset was successful, will fail if the timer is currently running/reset cancelled
 */
int ls_timer_reset(ls_timer* timer)
{
    // Disallow resets while running
    if (timer->running)
        return 0;

    if (timer->started && ls_timer_get_time(timer, true) <= 0) {
        return ls_timer_cancel(timer);
    }

    if (timer->curr_split < timer->game->split_count) {
        if (cfg.libresplit.save_run_history.value.b) {
            ls_run_save(timer, "RESET");
        }
    }

    // Warn if the reset will lose a gold split, and allow the user to cancel the reset if they want to keep it
    if (ls_timer_has_gold_split(timer)) {
        bool user_reset = true;
        if (cfg.libresplit.ask_on_gold.value.b) {
            user_reset = display_confirm_reset_dialog();
        }
        if (!user_reset) {
            return 0;
        }
    }

    reset_timer(timer);
    return 1;
}

/**
 * Cancels the current run, ignoring attempt and resetting timer
 *
 * @param timer The timer instance
 * @return Whether the cancel was successful, will fail if the timer is currently running
 */
int ls_timer_cancel(ls_timer* timer)
{
    // Disallow resets while running
    if (timer->running)
        return 0;

    if (timer->started) {
        if (*timer->attempt_count > 0) {
            --*timer->attempt_count;
        }
    }
    reset_timer(timer);
    return 1;
}
