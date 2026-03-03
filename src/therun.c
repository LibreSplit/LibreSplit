#include "therun.h"
#include "timer.h"
#include <curl/curl.h>
#include <jansson.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int64_t real_epoch_start_ms = 0;

/**
 * Converts a time in "milliseconds" (microseconds) to a json float in milliseconds.
 *
 * This should be put in timer.c later probably
 *
 * @param microseconds Time to convert.
 */
json_t* time_to_ms(int64_t microseconds)
{
    if (microseconds == LLONG_MAX) {
        return json_null();
    }
    double milliseconds = (double)microseconds / 1000.0;
    return json_real(milliseconds);
}

/** 0 - start
 *  1 - split
 *  2 - reset
 *  3 - pause
 *  4 - unpause
 *  5 - finish
 *  6 - undo
 *  7 - skip
 *
 * LiveSplit behavior at finish is to set currentSplitIndex to total amount of splits + 1
 * and currentSplitName to "", check what LibreSplit will do
 *
 * Undosplit should resend payload with splitTime reset to null, decrease currentSplitIndex
 * and update currentSplitName.
 */
char* build_therun_live_payload(ls_timer* timer, int source)
{
    const char* therun_key = getenv("LIBRESPLIT_THERUN_KEY");
    if (source == 0) {
        real_epoch_start_ms = (int64_t)time(NULL) * 1000;
    }
    json_t* root = json_object();

    json_t* metadata = json_object();
    // This is all to split split file title to game and category for API. Should be added as fields into
    // JSON at some point, alongside platform, region and emulator maybe?
    const char* game_title = timer->game->title;
    char* pipe_pos = strchr(game_title, '|');
    if (pipe_pos != NULL) {
        size_t game_len = pipe_pos - game_title;
        while (game_len > 0 && game_title[game_len - 1] == ' ') {
            game_len--;
        }
        json_object_set_new(metadata, "game", json_stringn(game_title, game_len));
        const char* category_start = pipe_pos + 1;
        while (*category_start == ' ') {
            category_start++;
        }
        json_object_set_new(metadata, "category", json_string(category_start));
    } else {
        json_object_set_new(metadata, "game", json_string(game_title));
        json_object_set_new(metadata, "category", json_string(""));
    }
    json_object_set_new(metadata, "platform", json_string(""));
    json_object_set_new(metadata, "region", json_string(""));
    json_object_set_new(metadata, "emulator", json_false());
    json_object_set_new(metadata, "variables", json_string("")); // Empty in my dumps
    json_object_set_new(root, "metadata", metadata);

    json_t* runData = json_array();
    for (unsigned int i = 0; i < timer->game->split_count; i++) {
        json_t* segment = json_object();
        json_object_set_new(segment, "name", json_string(timer->game->split_titles[i]));
        json_object_set_new(segment, "splitTime", time_to_ms(timer->split_times[i]));
        json_object_set_new(segment, "pbSplitTime", time_to_ms(timer->split_times[i])); // I believe these 3 are correct this way around?
        json_object_set_new(segment, "bestPossible", time_to_ms(timer->best_splits[i]));
        json_t* comparisons = json_array();
        json_t* personalbest = json_object();
        json_object_set_new(personalbest, "name", json_string("Personal Best"));
        json_object_set_new(personalbest, "time", time_to_ms(timer->split_times[i]));
        json_t* besttime = json_object();
        json_object_set_new(besttime, "name", json_string("Best Time"));
        json_object_set_new(besttime, "time", time_to_ms(timer->best_splits[i]));
        json_t* bestsegment = json_object();
        json_object_set_new(bestsegment, "name", json_string("Best Segment"));
        json_object_set_new(bestsegment, "time", time_to_ms(timer->best_segments[i]));
        json_array_append_new(comparisons, personalbest);
        json_array_append_new(comparisons, besttime);
        json_array_append_new(comparisons, bestsegment);
        json_object_set_new(segment, "comparisons", comparisons); // Best effort, neither Personal Best or Best Segment are 100% corrent with how LiveSplit does it, and Averages does not exist yet
        json_array_append_new(runData, segment);
        // fprintf(stderr, "%d", i);
    }
    json_object_set_new(root, "runData", runData);

    json_object_set_new(root, "timingMethod", json_integer(timer->usingGameTime)); // NYI, set in Compare Against option in RMB menu in LiveSplit, 0 for RTA, 1 for IGT
    if (timer->usingGameTime) {
        json_object_set_new(root, "currentTime", time_to_ms(timer->gameTime)); // This now changed because we have separate gameTime and realTime, redo this part
        json_object_set_new(root, "currentDuration", time_to_ms(timer->gameTime)); // NYI, Time with pauses, for now just time, also redo for gameTime/realTime
    } else {
        json_object_set_new(root, "currentTime", time_to_ms(timer->realTime)); // This now changed because we have separate gameTime and realTime, redo this part
        json_object_set_new(root, "currentDuration", time_to_ms(timer->realTime)); // NYI, Time with pauses, for now just time, also redo for gameTime/realTime
    }
    if (source == 2) {
        json_object_set_new(root, "currentSplitName", json_string(""));
        json_object_set_new(root, "currentSplitIndex", json_integer(-1));
    } else {
        json_object_set_new(root, "currentSplitName", json_string(timer->game->split_titles[timer->curr_split]));
        json_object_set_new(root, "currentSplitIndex", json_integer(timer->curr_split));
    }
    char start_time_str[32];
    snprintf(start_time_str, sizeof(start_time_str), "/Date(%lld)/", (long long)real_epoch_start_ms);
    json_object_set_new(root, "startTime", json_string(start_time_str)); // NYI, this is timestamp in ms, formatted as a string like "\/Date(1772038944242)\/"
    json_object_set_new(root, "endTime", json_integer(0)); // NYI, probably not needed, it ususally either "\/Date(-62135596800000)\/" or timestamp of last run finishing
    json_object_set_new(root, "uploadKey", json_string(therun_key));
    if (source == 3) {
        json_object_set_new(root, "isPaused", json_true());
        json_object_set_new(root, "isGameTimePaused", json_true());
        json_object_set_new(root, "wasJustResumed", json_false());
    } else if (source == 4) {
        json_object_set_new(root, "isPaused", json_false());
        json_object_set_new(root, "isGameTimePaused", json_false());
        json_object_set_new(root, "wasJustResumed", json_true());
    } else {
        json_object_set_new(root, "isPaused", json_false());
        json_object_set_new(root, "isGameTimePaused", json_false());
        json_object_set_new(root, "wasJustResumed", json_false());
    }
    json_object_set_new(root, "gameTimePauseTime", json_null()); // unused, for now null
    json_object_set_new(root, "totalPauseTime", json_null()); // unused, for now null
    json_object_set_new(root, "currentPauseTime", json_null()); // unused, for now null
    json_object_set_new(root, "timePausedAt", json_integer(0)); // unused?, for now 0
    json_object_set_new(root, "currentComparison", json_string("Personal Best")); // NYI, for now PB

    char* payload = json_dumps(root, 1);

    time_t rawtime;
    struct tm* timeinfo;
    char time_buf[64];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", timeinfo);
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "build/test-%s.json", time_buf);
    json_dump_file(root, filename, JSON_PRESERVE_ORDER | JSON_INDENT(2));
    json_decref(root);
    // fprintf(stderr, payload);
    // fprintf(stderr, "%d", source);

    return payload;
}

static size_t curl_discard_response(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    return size * nmemb;
}

void* therun_upload_thread(void* arg)
{
    char* payload = (char*)arg;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://dspc6ekj2gjkfp44cjaffhjeue0fbswr.lambda-url.eu-west-1.on.aws/");
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_discard_response);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            // fprintf(stderr, "[therun.gg] FAILED: %s\n", curl_easy_strerror(res));
        } else {
            // fprintf(stderr, "[therun.gg] OK!\n");
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    free(payload);
    return 0;
}

void therun_trigger_update(ls_timer* timer, int source)
{
    char* payload = build_therun_live_payload(timer, source);
    if (!payload)
        return;

    pthread_t thread_id;
    int result = pthread_create(&thread_id, NULL, therun_upload_thread, payload);
    if (result == 0) {
        pthread_detach(thread_id);
    } else {
        // fprintf(stderr, "[therun.gg] THREAD FAILED!\n");
        free(payload);
    }
}