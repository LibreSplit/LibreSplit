#pragma once

#include "timer.h"
#include "src/gui/app_window.h"

/**
 * MAX_ATTEMPTS_ARRAY_CAPACITTY ensures that at any given time
 * the maximum amount of memory our in-memory attempts history
 * will be MAX_ATTEMPTS_ARRAY_CAPACITTY * sizeof(ls_attempt)
 *
 * This memory is not all allocated up front, and the size of ls_attempt
 * is itself dynamic based on how many splits were completed before reset.
 * Given that, this would truly require the user to sit there doing *completed*
 * runs for years without ever saving or closing LibreSplit to reach theoreticaly
 * "large" memory usage in the hundreds of megabytes territory.
 */
#define INITIAL_ATTEMPTS_ARRAY_SIZE 16 // 2^4
#define MAX_ATTEMPTS_ARRAY_CAPACITTY 524288 // 2^19

/**
 * @brief This holds information about an attempt after it is complete.
 * By complete, I mean the attempt is no longer in `ls_timer` meaning the run
 * finished, was reset etc. This should only hold information from `ls_timer`
 * that actually goes into the user's saved attempt history.
 */
typedef struct ls_attempt {
    char* reason;
    size_t split_count;
    size_t curr_split;
    char** split_titles;
    ls_time* split_times;
    ls_time* segment_times;
    ls_time final_time;
    char start_time[64];
    char end_time[64];
} ls_attempt;

typedef struct ls_runs {
    ls_attempt** attempts; /**< The attempts array */
    size_t count; /**< The number of attempts in the array i.e. used slots */
    size_t size; /**< The current actual allocation size of the array i.e. total slots */
    char date[16]; /**< The date from when this session began for the attempts file */
} ls_runs;

int ls_runs_create(ls_runs** attempts);
void ls_runs_release(ls_runs* attempts);
bool ls_runs_append(ls_runs* self, ls_attempt* attempt);
bool ls_runs_clear(ls_runs* self);
ls_attempt* ls_runs_new_attempt(ls_timer* timer, const char* reason);
int ls_runs_save(const ls_runs* snapshot, const ls_game* game, GtkWindow* win);