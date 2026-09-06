#include "game.h"
#include "src/gui/component/components.h"
#include "src/gui/theming.h"
#include "src/logging.h"
#include "src/settings/definitions.h"
#include <gtk/gtk.h>

extern AppConfig cfg;

static GThread* save_thread;
static atomic_bool saving;

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
            goto snapshot_failed;
        }
    }

    if (game->theme) {
        snapshot->theme = strdup(game->theme);
        if (!snapshot->theme) {
            LOG_ERR("snapshot creation: unable to duplicate `theme` in memory");
            goto snapshot_failed;
        }
    }

    if (game->theme_variant) {
        snapshot->theme_variant = strdup(game->theme_variant);
        if (!snapshot->theme_variant) {
            LOG_ERR("snapshot creation: unable to duplicate `theme_variant` in memory");
            goto snapshot_failed;
        }
    }

    if (!game->split_count) {
        return snapshot;
    }

    if (!game->split_titles || !game->split_icon_paths || !game->split_times || !game->segment_times || !game->best_splits || !game->best_segments) {
        LOG_ERR("snapshot creation: positive split_count with empty split array(s)");
        goto snapshot_failed;
    }

    snapshot->split_titles = calloc(game->split_count, sizeof(char*));
    if (!snapshot->split_titles) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_titles`");
        goto snapshot_failed;
    }

    snapshot->split_icon_paths = calloc(game->split_count, sizeof(char*));
    if (!snapshot->split_icon_paths) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_icon_paths`");
        goto snapshot_failed;
    }

    snapshot->split_times = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->split_times) {
        LOG_ERR("snapshot creation: unable to allocate memory for `split_times`");
        goto snapshot_failed;
    }

    snapshot->segment_times = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->segment_times) {
        LOG_ERR("snapshot creation: unable to allocate memory for `segment_times`");
        goto snapshot_failed;
    }

    snapshot->best_splits = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->best_splits) {
        LOG_ERR("snapshot creation: unable to allocate memory for `best_splits`");
        goto snapshot_failed;
    }

    snapshot->best_segments = calloc(game->split_count, sizeof(ls_time));
    if (!snapshot->best_segments) {
        LOG_ERR("snapshot creation: unable to allocate memory for `best_segments`");
        goto snapshot_failed;
    }

    memcpy(snapshot->split_times, game->split_times, game->split_count * sizeof(ls_time));
    memcpy(snapshot->segment_times, game->segment_times, game->split_count * sizeof(ls_time));
    memcpy(snapshot->best_splits, game->best_splits, game->split_count * sizeof(ls_time));
    memcpy(snapshot->best_segments, game->best_segments, game->split_count * sizeof(ls_time));

    for (unsigned int i = 0; i < game->split_count; ++i) {
        if (game->split_titles[i]) {
            snapshot->split_titles[i] = strdup(game->split_titles[i]);
            if (!snapshot->split_titles[i]) {
                LOG_ERRF("snapshot creation: unable to duplicate `split_titles[%zu]` in memory", i);
                goto snapshot_failed;
            }
        }

        if (game->split_icon_paths[i]) {
            snapshot->split_icon_paths[i] = strdup(game->split_icon_paths[i]);
            if (!snapshot->split_icon_paths[i]) {
                LOG_ERRF("snapshot creation: unable to duplicate `split_icon_paths[%zu]` in memory", i);
                goto snapshot_failed;
            }
        }
    }

    return snapshot;

snapshot_failed:
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
    }

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->show_game) {
            component->ops->show_game(component, win->game, win->timer);
        }
    }

    gtk_widget_set_visible(win->box, TRUE);
    gtk_widget_set_visible(win->welcome_box->box, FALSE);
}

/**
 * @brief saves the game to the user's splits file. This function
 * should be asynchronous and run in its own thread.
 *
 * @param data A valid snapshot from `create_snapshot` of the current game state to save.
 * @return gpointer unused
 */
static gpointer save_game_thread(gpointer data)
{
    ls_game* snapshot = data;
    ls_game_save(snapshot);
    ls_game_release(snapshot);
    atomic_store(&saving, false);
    return NULL;
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
    if (!game || atomic_exchange(&saving, true)) {
        return;
    }

    if (save_thread) {
        g_thread_join(save_thread);
        save_thread = NULL;
    }

    ls_game* snapshot = create_snapshot(game);
    if (!snapshot) {
        atomic_store(&saving, false);
        return;
    }

    save_thread = g_thread_new("save_game", save_game_thread, snapshot);
}

/**
 * @brief Join the game save thread on exit.
 */
void save_game_join(void)
{
    if (save_thread) {
        g_thread_join(save_thread);
        save_thread = NULL;
    }

    atomic_store(&saving, false);
}
