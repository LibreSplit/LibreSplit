#include "runs.h"
#include "logging.h"
#include "src/gui/game.h"
#include "src/gui/widgets/dialog.h"
#include "src/settings/utils.h"
#include <string.h>
#include <sys/stat.h>

typedef enum LSGrowResult {
    LS_GROW_SUCCEEDED,
    LS_GROW_AT_MAX_CAPACITY,
    LS_GROW_REALLOC_FAILED,
} LSGrowResult;

/**
 * @brief Sets today's date to the date buffer in YYYY-MM-DD format.
 *
 * @param date Pointer to a string of at least length 16.
 * @return bool Whether or not fetching today's date was successful.
 */
static bool set_date(char* date)
{
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        LOG_WARNF("failed to set time: %s", g_strerror(errno));
        return false;
    }

    struct tm local_time;
    if (localtime_r(&now, &local_time) == NULL) {
        LOG_WARN("failed to format time in the user's locale");
        return false;
    }

    if (strftime(date, 16, "%Y-%m-%d", &local_time) == 0) {
        LOG_WARN("failed to store the formatted time in the date buffer, the result might be longer than date's size");
        return false;
    }

    return true;
}

/**
 * @brief Creates a new attempts array and assigns it to
 * the pointers in runs.
 *
 * @param runs Pointer to the memory location to store the new array
 * @return int 0 on success otherwise failure
 */
int ls_runs_create(ls_runs** runs)
{
    int error = 0;
    ls_runs* self = calloc(1, sizeof(ls_runs));
    if (self == NULL) {
        error = 1;
        LOG_WARN("failed to allocate memory for `ls_runs`");
        goto ls_runs_create_error;
    }

    if (!set_date(self->date)) {
        error = 1;
        goto ls_runs_create_error;
    }

    self->attempts = calloc(INITIAL_ATTEMPTS_ARRAY_SIZE, sizeof(ls_attempt*));
    if (self->attempts == NULL) {
        error = 1;
        LOG_WARN("failed to allocate memory for the `ls_runs` attempts array");
        goto ls_runs_create_error;
    }

    self->size = INITIAL_ATTEMPTS_ARRAY_SIZE;
    self->count = 0;

ls_runs_create_error:
    if (error) {
        if (self) {
            ls_runs_release(self);
        }

        return error;
    }

    // free old runs before replacing.
    if (*runs) {
        ls_runs_release(*runs);
        *runs = 0;
    }

    *runs = self;
    return 0;
}

/**
 * @brief Frees all attempt data for an individual attempt
 *
 * @param attempt the attempt instance
 */
static void ls_attempt_release(ls_attempt* attempt)
{
    if (attempt == NULL) {
        return;
    }

    free(attempt->split_times);
    free(attempt->segment_times);
    free(attempt->reason);

    if (attempt->split_titles) {
        for (unsigned int i = 0; i < attempt->curr_split; ++i) {
            free(attempt->split_titles[i]);
        }
    }

    free(attempt->split_titles);
    free(attempt);
}

/**
 * @brief Frees all runs data
 *
 * @param self the runs instance
 */
void ls_runs_release(ls_runs* self)
{
    for (size_t i = 0; i < self->count; i++) {
        ls_attempt_release(self->attempts[i]);
    }

    free(self->attempts);
    self->count = 0;
    self->size = 0;

    free(self);
}

/**
 * @brief Resizes the dynamic array at a rate of 1.5x its current size
 *
 * @param self The runs instance
 * @return bool Whether or not the reallocation succeeded
 */
static LSGrowResult ls_attempts_grow(ls_runs* self)
{
    if (self->size >= MAX_ATTEMPTS_ARRAY_CAPACITTY) {
        return LS_GROW_AT_MAX_CAPACITY;
    }

    size_t new_size = self->size + ((size_t)(self->size / 2));
    if (new_size > MAX_ATTEMPTS_ARRAY_CAPACITTY) {
        new_size = MAX_ATTEMPTS_ARRAY_CAPACITTY;
    }

    size_t old_size = self->size;
    ls_attempt** new_attempts = realloc(self->attempts, new_size * sizeof(ls_attempt*));
    if (new_attempts == NULL) {
        LOG_WARNF("unable to reallocate runs to new size of: %zu", new_size);
        return LS_GROW_REALLOC_FAILED;
    }

    self->attempts = new_attempts;
    self->size = new_size;

    // NULL new memory block
    memset(self->attempts + old_size, 0, (new_size - old_size) * sizeof(ls_attempt*));
    return LS_GROW_SUCCEEDED;
}

/**
 * @brief Callback handler for the failure alert of `ls_runs_clear`
 * to close LibreSplit.
 *
 * @param data unused
 * @param gboolean always G_SOURCE_REMOVE
 */
static gboolean ls_runs_clear_failure(gpointer data)
{
    gtk_window_destroy(GTK_WINDOW(ls_get_main_app_window()));
    return G_SOURCE_REMOVE;
}

/**
 * @brief Wrapper for ls_runs_clear for internal dialog callbacks
 * with error handling.
 *
 * @param data Pointer to self.
 * @return gboolean void in practice, gboolean for GSourceFunc
 */
static gboolean ls_runs_clear_callback(gpointer data)
{
    ls_runs* self = data;
    if (!ls_runs_clear(self)) {
        // this callback is on the main gtk thread, safe to fetch win singleton
        ls_runs_clear_failed(GTK_WINDOW(ls_get_main_app_window()));
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Wrapper for ls_runs_clear_callback that saves the user's run first.
 *
 * @param data Pointer to self
 * @return gboolean void in practice, gboolean for GSourceFunc
 */
static gboolean ls_runs_clear_callback_with_save(gpointer data)
{
    ls_runs* self = data;
    LSAppWindow* win = ls_get_main_app_window();
    save_game(win->game);
    save_game_join(false);
    ls_runs_clear_callback(self);
    return G_SOURCE_REMOVE;
}

/**
 * @brief On ls_runs_clear failure, shows an error to the user with recovery attempts.
 *
 * @param self The runs instance.
 * @param win The main window, used for parenting any potential error dialogs.
 */
static void ls_runs_clear_failure_show(ls_runs* self, GtkWindow* win)
{
    const LSDialogIcon icon = {
        .source = "dialog-warning",
        .type = LS_DIALOG_ICON_NAME,
    };

    const LSDialogOption options[] = {
        {
            .label = "_Yes",
            .callback = ls_runs_clear_callback_with_save,
            .is_cancel = FALSE,
            .is_default = TRUE,
        },
        {
            .label = "_No",
            .callback = ls_runs_clear_callback,
            .is_cancel = TRUE,
            .is_default = FALSE,
        }
    };

    if (!ls_dialog_open(win,
            "LibreSplit",
            "Warning: Runs at Capacity",
            "You have reached the maximum capacity of attempts we store in memory (that's... impressive...)\n"
            "To prevent unnecessary RAM usage we will now clear your attempts and any unsaved data will be lost.\n"
            "Would you like us to save your splits now first?",
            &icon,
            options,
            G_N_ELEMENTS(options), self, NULL)) {
        // We don't even have memory for a dialog, sorry your data is gone
        if (!ls_runs_clear(self)) {
            g_idle_add_full(G_PRIORITY_HIGH, ls_runs_clear_failure, NULL, NULL);
        }
    }
}

/**
 * @brief We couldn't show the user the dialog so err on the side of saving their data if we can.
 * This function is called through the GTK main thread.
 *
 * @param data Pointer to self.
 * @return gboolean void in practice, gboolean for GSourceFunc
 */
static gboolean ls_runs_save_and_clear(gpointer data)
{
    ls_runs* self = data;
    save_game(ls_get_main_app_window()->game);
    save_game_join(false);

    if (!ls_runs_clear(self)) {
        // Well, we tried.
        g_idle_add_full(G_PRIORITY_HIGH, ls_runs_clear_failure, NULL, NULL);
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief On ls_attempts_grow reallocation failure, shows an error to the user with recovery attempts.
 * This should be an exceedingly rare occurance under extreme circumstances.
 * Recovery is best effort but can not be guaranteed at this point.
 *
 * @param self The runs instance.
 * @param win The main window, used for parenting any potential error dialogs.
 */
static void ls_attempts_realloc_failure_show(ls_runs* self, GtkWindow* win)
{
    const LSDialogIcon icon = {
        .source = "dialog-warning",
        .type = LS_DIALOG_ICON_NAME,
    };

    const LSDialogOption options[] = {
        {
            .label = "_Yes",
            .callback = ls_runs_clear_callback_with_save,
            .is_cancel = FALSE,
            .is_default = TRUE,
        },
        {
            .label = "_No",
            .callback = ls_runs_clear_callback,
            .is_cancel = TRUE,
            .is_default = FALSE,
        }
    };

    if (!ls_dialog_open(win,
            "LibreSplit",
            "Warning: Unable to Store New Attempts",
            "We're currently unable to store any new unsaved attempts and need to clear your current unsaved attempts.\n"
            "Would you like to save your splits first?",
            &icon,
            options,
            G_N_ELEMENTS(options), self, NULL)) {
        // We don't even have memory for a dialog, let's try to save and clear.
        g_idle_add_full(G_PRIORITY_HIGH, ls_runs_save_and_clear, self, NULL);
    }
}

/**
 * @brief Appends the attempt to the next empty slot in the array
 * and increments the count. Automatically grows the array
 * when nearing capacity.
 *
 * Always appends the entry to the array even if growth fails.
 * When growth fails we should prevent new runs since storing the
 * attempt after that point becomes impossible.
 *
 * @param self The runs instance.
 * @param attempt The attempt instance to append to runs.
 * @param win The main window, used for parenting any potential error dialogs.
 */
void ls_runs_append(ls_runs* self, ls_attempt* attempt, GtkWindow* win)
{
    self->attempts[self->count++] = attempt;
    if (self->count == self->size) {
        LSGrowResult result = ls_attempts_grow(self);
        switch (result) {
            case LS_GROW_SUCCEEDED:
                break;
            case LS_GROW_AT_MAX_CAPACITY:
                ls_runs_clear_failure_show(self, win);
                break;
            case LS_GROW_REALLOC_FAILED:
                ls_attempts_realloc_failure_show(self, win);
                break;
        }
    }
}

/**
 * @brief Clears the array and reduces memory usage.
 *
 * @param self The current runs instance.
 * @return bool Whether or not the clear succeeded.
 */
bool ls_runs_clear(ls_runs* self)
{
    for (size_t i = 0; i < self->count; i++) {
        ls_attempt_release(self->attempts[i]);
    }

    free(self->attempts);
    self->size = 0;
    self->count = 0;

    self->attempts = calloc(INITIAL_ATTEMPTS_ARRAY_SIZE, sizeof(ls_attempt*));
    if (self->attempts == NULL) {
        // This should never happens since we should have freed more memory than we're requesting.
        LOG_WARN("unable to allocate runs after clear");
        return false;
    }

    self->size = INITIAL_ATTEMPTS_ARRAY_SIZE;
    self->count = 0;
    return true;
}

/**
 * @brief Creates a new persistent attempt based on the current timer state
 * and timer completion reason.
 *
 * @param timer The current timer instance.
 * @param reason The reason for the run termination.
 * @return ls_attempt*
 */
ls_attempt* ls_runs_new_attempt(ls_timer* timer, const char* reason)
{
    ls_time final_time = ls_timer_get_time(timer, true);
    if (ls_time_lte_zero(final_time)) {
        return NULL;
    }

    if (reason == NULL) {
        LOG_WARN("invalid NULL reason provided");
        return NULL;
    }

    ls_attempt* attempt = calloc(1, sizeof(ls_attempt));
    if (attempt == NULL) {
        LOG_WARN("unable to allocate a new attempt");
        goto ls_runs_new_attempt_failed;
    }

    attempt->split_count = timer->game->split_count;
    const size_t curr_split = timer->curr_split;
    const size_t time_size = curr_split * sizeof(ls_time);

    attempt->curr_split = curr_split;
    strcpy(attempt->start_time, timer->start_time);
    ls_run_set_time(attempt->end_time);

    attempt->split_times = calloc(1, time_size);
    if (attempt->split_times == NULL) {
        LOG_WARN("unable to allocate `split_times` for the attempt");
        goto ls_runs_new_attempt_failed;
    }

    attempt->segment_times = calloc(1, time_size);
    if (attempt->segment_times == NULL) {
        LOG_WARN("unable to allocate `segment_times` for the attempt");
        goto ls_runs_new_attempt_failed;
    }

    attempt->reason = strdup(reason);
    if (attempt->reason == NULL) {
        LOG_WARN("unable to duplicate `reason` for the attempt");
        goto ls_runs_new_attempt_failed;
    }

    attempt->split_titles = calloc(1, curr_split * sizeof(char*));
    if (attempt->split_titles == NULL) {
        LOG_WARN("unable to allocate `segment_times` for the attempt");
        goto ls_runs_new_attempt_failed;
    }

    for (unsigned int i = 0; i < curr_split; ++i) {
        // Accept empty split titles before trying to allocate memory for them.
        if (timer->game->split_titles[i] == NULL) {
            attempt->split_titles[i] = NULL;
            continue;
        }

        attempt->split_titles[i] = strdup(timer->game->split_titles[i]);
        if (attempt->split_titles[i] == NULL) {
            LOG_WARNF("unable to duplicate `split_titles[%u]` for the attempt", i);
            goto ls_runs_new_attempt_failed;
        }
    }

    memcpy(attempt->split_times, timer->split_times, time_size);
    memcpy(attempt->segment_times, timer->segment_times, time_size);
    attempt->final_time = final_time;
    return attempt;

ls_runs_new_attempt_failed:
    ls_attempt_release(attempt);
    return NULL;
}

static json_t* get_or_create_runs_history(const ls_game* game, const char* date, char* path, GtkWindow* win, json_error_t* json_error)
{
    const char* name = strrchr(game->path, '/');
    name = name ? name + 1 : game->path;
    const char* dot = strrchr(name, '.');

    bool next_to_splits = cfg.libresplit.run_history_next_to_splits.value.b;
    const char* base = next_to_splits ? game->path : name;
    size_t len = strlen(base);
    if (dot && dot != name && strcmp(name, "..") != 0) {
        len = (size_t)(dot - base);
    }

    int written;
    if (next_to_splits) {
        written = snprintf(path, PATH_MAX, "%.*s", (int)len, base);
    } else {
        char libresplit_directory[PATH_MAX];
        get_libresplit_folder_path(libresplit_directory);
        written = snprintf(path, PATH_MAX, "%s/runs/%.*s", libresplit_directory, (int)len, base);
    }

    if (written < 0 || written >= PATH_MAX) {
        if (written < 0) {
            LOG_ERRF("save game: error determining run histories save location: %s", g_strerror(errno));
        } else {
            LOG_ERR("save game: run histories save location is too long");
        }

        return NULL;
    }

    len = (size_t)written;
    if (!create_default_directory(game->title ? game->title : "runs history directory", path, 0755, win)) {
        return NULL;
    }

    written = snprintf(path + len, PATH_MAX - len, "/%s.json", date);
    if (written < 0 || (size_t)written >= PATH_MAX - len) {
        if (written < 0) {
            LOG_ERRF("save game: error determining run histories save location: %s", g_strerror(errno));
        } else {
            LOG_ERR("save game: run histories save location is too long");
        }

        return NULL;
    }

    struct stat st = { 0 };
    if (stat(path, &st) == -1) {
        return json_array();
    }

    json_t* json = json_load_file(path, 0, json_error);
    if (!json_is_array(json)) {
        json_decref(json);
        return json_array();
    }

    return json;
}

/**
 * Saves the current runs history snapshot to today's runs file.
 *
 * @param snapshot The runs snapshot to save.
 * @param game The current game instance.
 * @param win The current gtk window instance.
 * @return bool Whether or not the save succeeded.
 */
bool ls_runs_save(const ls_runs* snapshot, const ls_game* game, GtkWindow* win)
{
    LOG_DEBUG("Saving attempts history...");

    char path[PATH_MAX];
    json_error_t json_error = { 0 };
    json_t* runs = get_or_create_runs_history(game, snapshot->date, path, win, &json_error);
    if (!runs) {
        if (json_error.line) {
            LOG_ERRF("%s (%d:%d)", json_error.text, json_error.line, json_error.column);
        }

        return false;
    }

    for (size_t i = 0; i < snapshot->count; ++i) {
        ls_attempt* attempt = snapshot->attempts[i];

        // Root JSON Object
        json_t* json = json_object();
        json_t* final = json_object();
        json_time_set(final, &attempt->final_time);
        json_object_set_new(json, "start_time", json_string(attempt->start_time));
        json_object_set_new(json, "end_time", json_string(attempt->end_time));
        json_object_set_new(json, "final_time", final);
        json_object_set_new(json, "reason", json_string(attempt->reason));

        // Splits Array
        json_t* splits = json_array();

        for (size_t j = 0; j < attempt->curr_split; ++j) {
            json_t* split = json_object();

            // Title
            json_object_set_new(split, "title", json_string(attempt->split_titles[j]));

            // Check if time is valid, avoids saving time on skipped splits
            if (is_time_valid(attempt->split_times[j].game_time) && is_time_valid(attempt->split_times[j].real_time)) {
                json_t* time = json_object();
                json_time_set(time, &attempt->split_times[j]);
                json_object_set_new(split, "time", time);
                // Check if segment time is valid, avoids saving segment time AFTER skipped split
                if (is_time_valid(attempt->segment_times[j].game_time) && is_time_valid(attempt->segment_times[j].real_time)) {
                    json_t* segment = json_object();
                    json_time_set(segment, &attempt->segment_times[j]);
                    json_object_set_new(split, "segment", segment);
                } else {
                    json_object_set_new(split, "segment", json_null());
                }
            } else {
                json_object_set_new(split, "time", json_null());
                json_object_set_new(split, "segment", json_null());
            }

            json_array_append_new(splits, split);
        }

        json_object_set_new(json, "splits", splits);
        json_array_append_new(runs, json);
    }

    bool success = ls_write_save(runs, path);
    json_decref(runs);
    return success;
}

/**
 * @brief Handles a run clear failure by informing the user what happened
 * and then closing the app.
 *
 * @param win The current window instance.
 */
void ls_runs_clear_failed(GtkWindow* win)
{
    LOG_WARN("Runs history creation failed after clear - Closing LibreSplit");
    const LSDialogIcon icon = {
        .source = "dialog-warning",
        .type = LS_DIALOG_ICON_NAME,
    };

    const LSDialogOption options[] = {
        {
            .label = "_OK",
            .callback = ls_runs_clear_failure,
            .is_cancel = FALSE,
            .is_default = TRUE,
            .priority = G_PRIORITY_HIGH,
        }
    };

    if (!ls_dialog_open(win,
            "LibreSplit",
            "Unable to initialize new run history",
            "Your run history saved successfully however we were unable to prepare LibreSplit for new runs.\n"
            "LibreSplit will now close to prevent any corruption.",
            &icon, options, G_N_ELEMENTS(options), NULL, NULL)) {
        // We couldn't even create a dialog, so just close.
        g_idle_add_full(G_PRIORITY_HIGH, ls_runs_clear_failure, NULL, NULL);
    }
}