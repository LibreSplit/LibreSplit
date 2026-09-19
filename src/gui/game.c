#include "game.h"
#include "src/gui/component/components.h"
#include "src/gui/theming.h"
#include "src/gui/widgets/alert.h"
#include "src/logging.h"
#include "src/runs.h"
#include "src/settings/definitions.h"
#include <gtk/gtk.h>
#include <unistd.h>

extern AppConfig cfg;

static GThread* save_thread;
static atomic_bool saving;
static GMutex save_mutex;
static bool saving_enabled = true;

// start as true to symbolize there has not yet been any failure
static atomic_bool last_game_save_result = true;
static atomic_bool last_runs_save_result = true;

typedef struct save_data {
    ls_game* game;
    ls_runs* runs;
    GWeakRef main_win;
} save_data;

/**
 * @brief Duplicates the ls_game as snapshot. This is useful
 * to allow for asynchronous game saves to perform the save operation
 * without worry that the ls_game struct might change in the middle
 * of the operation.
 *
 * @param game The ls_game struct to duplicate
 * @return ls_game* A pointer to the snapshot - must be independently ls_game_release'd when done with it
 */
static ls_game* create_snapshot(const ls_game* game)
{
    ls_game* snapshot = calloc(1, sizeof(ls_game));
    if (!snapshot) {
        LOG_ERR("snapshot creation: unable to allocate memory for `ls_game`")
        return NULL;
    }

    memcpy(snapshot->path, game->path, sizeof(game->path));
    snapshot->comparison_method = game->comparison_method;
    snapshot->attempt_count = game->attempt_count;
    snapshot->finished_count = game->finished_count;
    snapshot->width = game->width;
    snapshot->height = game->height;
    snapshot->world_record = game->world_record;
    snapshot->start_delay = game->start_delay;
    snapshot->contains_icons = game->contains_icons;
    snapshot->split_count = game->split_count;

    if (game->title) {
        snapshot->title = strdup(game->title);
        if (!snapshot->title) {
            LOG_ERR("snapshot creation: unable to duplicate `title` in memory");
            goto create_snapshot_failed;
        }
    }

    if (game->theme) {
        snapshot->theme = strdup(game->theme);
        if (!snapshot->theme) {
            LOG_ERR("snapshot creation: unable to duplicate `theme` in memory");
            goto create_snapshot_failed;
        }
    }

    if (game->theme_variant) {
        snapshot->theme_variant = strdup(game->theme_variant);
        if (!snapshot->theme_variant) {
            LOG_ERR("snapshot creation: unable to duplicate `theme_variant` in memory");
            goto create_snapshot_failed;
        }
    }

    if (game->auto_splitter_file) {
        snapshot->auto_splitter_file = strdup(game->auto_splitter_file);
        if (!snapshot->auto_splitter_file) {
            LOG_ERR("snapshot creation: unable to duplicate `auto_splitter_file` in memory");
            goto create_snapshot_failed;
        }
    }

    if (game->auto_splitter_settings_count) {
        lock_user_settings();
        snapshot->auto_splitter_settings_count = game->auto_splitter_settings_count;
        snapshot->auto_splitter_settings = calloc(game->auto_splitter_settings_count, sizeof(UserSetting*));
        if (!snapshot->auto_splitter_settings) {
            LOG_ERR("snapshot creation: snapshot creation: unable to allocate memory for `auto_splitter_settings`");
            unlock_user_settings();
            goto create_snapshot_failed;
        }

        for (size_t i = 0; i < game->auto_splitter_settings_count; ++i) {
            snapshot->auto_splitter_settings[i] = calloc(1, sizeof(UserSetting));
            if (!snapshot->auto_splitter_settings[i]) {
                LOG_ERRF("snapshot creation: snapshot creation: unable to allocate memory for setting[%zu]", i);
                unlock_user_settings();
                goto create_snapshot_failed;
            }

            snapshot->auto_splitter_settings[i]->key = strdup(game->auto_splitter_settings[i]->key);
            if (!snapshot->auto_splitter_settings[i]->key) {
                LOG_ERRF("snapshot creation: snapshot creation: unable to duplicate setting[%zu].key", i);
                unlock_user_settings();
                goto create_snapshot_failed;
            }

            snapshot->auto_splitter_settings[i]->type = game->auto_splitter_settings[i]->type;
            snapshot->auto_splitter_settings[i]->val = game->auto_splitter_settings[i]->val;
            if (snapshot->auto_splitter_settings[i]->type == SETTING_STRING) {
                snapshot->auto_splitter_settings[i]->val.string_val = strdup(game->auto_splitter_settings[i]->val.string_val);
                if (!snapshot->auto_splitter_settings[i]->val.string_val) {
                    LOG_ERRF("snapshot creation: snapshot creation: unable to duplicate setting[%zu] string value", i);
                    unlock_user_settings();
                    goto create_snapshot_failed;
                }
            }
        }
    }

    if (!game->split_count) {
        return snapshot;
    }

    if (!game->split_titles || !game->split_icon_paths || !game->split_times || !game->segment_times || !game->best_splits || !game->best_segments) {
        LOG_ERR("snapshot creation: positive split_count with empty split array(s)");
        goto create_snapshot_failed;
    }

    snapshot->split_titles = calloc(game->split_count, sizeof(char*));
    if (!snapshot->split_titles) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_titles`");
        goto create_snapshot_failed;
    }

    snapshot->split_icon_paths = calloc(game->split_count, sizeof(char*));
    if (!snapshot->split_icon_paths) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_icon_paths`");
        goto create_snapshot_failed;
    }

    snapshot->split_times = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->split_times) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_times`");
        goto create_snapshot_failed;
    }

    snapshot->segment_times = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->segment_times) {
        LOG_ERR("snapshot creation: unable to allocate memory for `segment_times`");
        goto create_snapshot_failed;
    }

    snapshot->best_splits = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->best_splits) {
        LOG_ERR("snapshot creation: unable to allocate memory for `best_splits`");
        goto create_snapshot_failed;
    }

    snapshot->best_segments = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->best_segments) {
        LOG_ERR("snapshot creation: unable to allocate memory for `best_segments`");
        goto create_snapshot_failed;
    }

    memcpy(snapshot->split_times, game->split_times, game->split_count * sizeof(ls_time));
    memcpy(snapshot->segment_times, game->segment_times, game->split_count * sizeof(ls_time));
    memcpy(snapshot->best_splits, game->best_splits, game->split_count * sizeof(ls_time));
    memcpy(snapshot->best_segments, game->best_segments, game->split_count * sizeof(ls_time));

    for (unsigned int i = 0; i < game->split_count; ++i) {
        if (game->split_titles[i]) {
            snapshot->split_titles[i] = strdup(game->split_titles[i]);
            if (!snapshot->split_titles[i]) {
                LOG_ERRF("snapshot creation: unable to duplicate `split_titles[%u]` in memory", i);
                goto create_snapshot_failed;
            }
        }

        if (game->split_icon_paths[i]) {
            snapshot->split_icon_paths[i] = strdup(game->split_icon_paths[i]);
            if (!snapshot->split_icon_paths[i]) {
                LOG_ERRF("snapshot creation: unable to duplicate `split_icon_paths[%u]` in memory", i);
                goto create_snapshot_failed;
            }
        }
    }

    return snapshot;

create_snapshot_failed:
    ls_game_release(snapshot);
    return NULL;
}

/**
 * Clears the current game and reset all the components.
 *
 * @param win The LibreSplit app window
 */
void ls_app_window_clear_game(LSAppWindow* win)
{
    LOG_DEBUG("Clearing Game...");
    GList* l;

    gtk_widget_set_visible(win->box, FALSE);
    gtk_widget_set_visible(win->welcome_box->box, TRUE);

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->clear_game) {
            component->ops->clear_game(component);
        }
    }

    ls_app_load_theme_with_fallback(win, cfg.libresplit.theme.value.s, cfg.libresplit.theme_variant.value.s);
}

/**
 * Prepares the LibreSplit window to be shown, using the data
 * from the loaded split file.
 *
 * @param win The LibreSplit window.
 */
void ls_app_window_show_game(LSAppWindow* win)
{
    LOG_DEBUG("Showing Game...");
    GList* l;

    // set dimensions
    if (win->game->width > 0 && win->game->height > 0) {
        // First set the "minimum size" allowed
        gtk_widget_set_size_request(GTK_WIDGET(win),
            win->game->width,
            win->game->height);
        // Then automatically resize the window to the preferences
        gtk_window_set_default_size(GTK_WINDOW(win),
            win->game->width,
            win->game->height);
        // User will still be able to resize the window up, but not down
    }

    // set game theme (if it is set)
    if (win->game->theme) {
        ls_app_load_theme_with_fallback(win, win->game->theme, win->game->theme_variant);
    } else {
        ls_app_load_theme_with_fallback(win, cfg.libresplit.theme.value.s, cfg.libresplit.theme_variant.value.s);
    }

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->show_game) {
            component->ops->show_game(component, win->game, win->timer);
        }
    }

    ls_app_window_draw(win);
    gtk_widget_set_visible(win->box, TRUE);
    gtk_widget_set_visible(win->welcome_box->box, FALSE);
}

/**
 * @brief saves the game to the user's splits file. This function
 * should be asynchronous and run in its own thread.
 *
 * @param data save_data containing a valid snapshot from `create_snapshot` of the current game state to save
 *              along with an optional (live) pointer to unsaved runs history.
 * @return gpointer unused
 */
static gpointer save_game_thread(gpointer data)
{
    save_data* snapshot = data;
    GObject* main_win = g_weak_ref_get(&snapshot->main_win);
    GtkWindow* win = main_win ? GTK_WINDOW(main_win) : NULL;

    bool runs_result = true;
    int result = ls_game_save(snapshot->game);
    if (result) {
        ls_alert_warning(win, "Save Failed", "Save Failed", "We were unable to save your game.\nIf this continues check your logs for errors.");
        goto save_game_thread_finished;
    }

    if (snapshot->runs) {
        runs_result = ls_runs_save(snapshot->runs, snapshot->game, win);
        if (!runs_result) {
            ls_alert_warning(win, "Save Failed", "Save Failed", "We were unable to save your runs history.\nIf this continues check your logs for errors.");
            goto save_game_thread_finished;
        }

        // This should not be possible to fail, if it does we end up with a broken state so close LibreSplit
        if (!ls_runs_clear(snapshot->runs)) {
            ls_runs_clear_failed(win);
        }
    }

save_game_thread_finished:
    atomic_store(&last_game_save_result, result == 0);
    atomic_store(&last_runs_save_result, runs_result);

    // if the game saved successfully, call ls_game_saved event.
    if (result == 0 && main_win) {
        ls_game_saved(LS_APP_WINDOW(main_win)->game);
    }

    g_clear_object(&main_win);
    g_weak_ref_clear(&snapshot->main_win);
    ls_game_release(snapshot->game);
    free(snapshot);

    atomic_store(&saving, false);
    ls_app_window_set_blocked(FALSE);
    return NULL;
}

bool is_saving(void)
{
    return atomic_load(&saving);
}

bool get_last_game_save_result(void)
{
    return atomic_load(&last_game_save_result);
}

bool get_last_runs_save_result(void)
{
    return atomic_load(&last_runs_save_result);
}

/**
 * @brief Atomically saves the ls_game struct to the user's splits file.
 * This function rejects new saves while another save is already running.
 * It creates a snapshot of the current save, then runs the save on the snapshot
 * via a worker thread.
 *
 * @param game The current ls_game struct to save
 */
void save_game(ls_game* game)
{
    if (!game) {
        LOG_WARN("Rejecting NULL game save");
        return;
    }

    g_mutex_lock(&save_mutex);
    if (!saving_enabled || atomic_exchange(&saving, true)) {
        LOG_WARN("Rejecting game save during shutdown or while another save is already occurring");
        g_mutex_unlock(&save_mutex);
        return;
    }

    if (save_thread) {
        g_thread_join(save_thread);
        save_thread = NULL;
    }

    save_data* snapshot = calloc(1, sizeof(save_data));
    if (snapshot == NULL) {
        LOG_WARN("unable to allocate memory for the save_data wrapper struct");
        atomic_store(&saving, false);
        g_mutex_unlock(&save_mutex);
        return;
    }

    snapshot->game = create_snapshot(game);
    if (snapshot->game == NULL) {
        free(snapshot);
        atomic_store(&saving, false);
        g_mutex_unlock(&save_mutex);
        return;
    }

    LSAppWindow* win = ls_get_main_app_window();
    g_weak_ref_init(&snapshot->main_win, win ? G_OBJECT(win) : NULL);

    if (cfg.libresplit.save_run_history.value.b) {
        snapshot->runs = win->runs;
    }

    // This should be imperceivable but block on save in case the user's machine is slow.
    ls_app_window_set_blocked(TRUE);
    save_thread = g_thread_new("save_game", save_game_thread, snapshot);
    g_mutex_unlock(&save_mutex);
}

/**
 * @brief Join the game save thread.
 */
void save_game_join(bool exiting)
{
    g_mutex_lock(&save_mutex);

    if (exiting) {
        // once we are exiting, prevent saves.
        saving_enabled = false;
    }

    if (save_thread) {
        g_thread_join(save_thread);
        save_thread = NULL;
    }

    g_mutex_unlock(&save_mutex);
}
