#include "src/gui/actions.h"
#include "gio/gio.h"
#include "src/gui/app_window.h"
#include "src/gui/backends/x11.h"
#include "src/gui/dialogs.h"
#include "src/gui/game.h"
#include "src/gui/timer.h"
#include "src/gui/widgets/alert.h"
#include "src/gui/widgets/dialog.h"
#include "src/lasr/auto-splitter.h"
#include "src/lasr/utils.h"
#include "src/logging.h"
#include "src/settings/settings.h"
#include <gtk/gtk.h>
#include <stdatomic.h>
#include <sys/stat.h>

/**
 * Compares the current timer and the saved one to see
 * if the current one is better for the game's comparison method.
 *
 * Ported from paoloose/urn @7456bfe
 *
 * @param game The current timer
 * @param timer The previous timer
 *
 * @return True if the current timer is better
 */
bool ls_is_timer_better(ls_game* game, ls_timer* timer)
{
    int i;
    long long timer_split_time = LLONG_MAX;
    long long game_split_time = LLONG_MAX;

    // Find the latest split with a time
    for (i = game->split_count - 1; i >= 0; i--) {
        timer_split_time = ls_time_get_by_method(timer->split_times[i], game->comparison_method);
        game_split_time = ls_time_get_by_method(game->split_times[i], game->comparison_method);
        if (timer_split_time != 0ll || game_split_time != 0ll) {
            break;
        }
    }

    if (i < 0) {
        return true;
    }
    if (timer_split_time == 0ll) {
        return false;
    }
    if (game_split_time == 0ll) {
        return true;
    }

    return timer_split_time <= game_split_time;
}

/**
 * @brief Callback function for when a valid local file is selected as the new splits file.
 * It is safe to assume that parent is the LSAppWindow becuase the LSAppWindow is passed to
 * ls_file_picker_open from the implementer. This assumption must not change.
 *
 * @param parent The main app window
 * @param filename The successfully selected local file
 */
static void open_splits_selected(GtkWindow* parent, const char* filename)
{
    LSAppWindow* win = LS_APP_WINDOW(parent);
    // Timer started while picking file sanity check
    if (win->timer && win->timer->running) {
        ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing splits.");
    } else {
        const char* folder_path = g_path_get_dirname(filename);
        CFG_SET_STR(cfg.history.last_split_folder.value.s, folder_path);
        ls_app_window_open(win, filename);
        CFG_SET_STR(cfg.history.split_file.value.s, filename);

        g_free(folder_path);
        config_save();
    }

    if (!win->game || !win->timer) {
        gtk_widget_set_visible(win->welcome_box->box, TRUE);
    }
}

/**
 * Shows the "Open JSON Split File" dialog eventually using
 * the last known split folder. Also saves a new "last used split folder".
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void open_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    char splits_path[PATH_MAX];
    LSAppWindow* win;
    struct stat st = { 0 };

    // Load the last used split folder, if present
    const char* last_split_folder = cfg.history.last_split_folder.value.s;
    if (parameter != NULL) {
        app = parameter;
    }

    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    if (win->timer && win->timer->running) {
        ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing splits.");
        return;
    }

    bool use_default_path = true;
    if (last_split_folder != NULL && last_split_folder[0] != '\0') {
        strcpy(splits_path, last_split_folder);

        // Just use the last saved path if it exists
        if (stat(splits_path, &st) == 0) {
            use_default_path = false;
        }
    }

    if (use_default_path) {
        // We have no saved path or the path no longer exists, go to the default splits path and eventually create it
        strcpy(splits_path, win->data_path);
        strcat(splits_path, "/splits");
        if (stat(splits_path, &st) == -1) {
            mkdir(splits_path, 0700);
        }
    }

    if (!win->game || !win->timer) {
        gtk_widget_set_visible(win->welcome_box->box, TRUE);
    }

    const LSFilePickerFilter filters[] = { { .name = "LibreSplit JSON Split File", .pattern = "*.json", .is_default = true } };
    const LSFilePickerOptions options = {
        .title = "Open Splits File",
        .path = splits_path,
        .filters = filters,
        .filters_count = G_N_ELEMENTS(filters),
    };

    ls_file_picker_open(GTK_WINDOW(win), &options, open_splits_selected);
}

static void perform_save_splits(gpointer window)
{
    LSAppWindow* win = LS_APP_WINDOW(window);
    ls_game_update_splits(win->game, win->timer);
    save_game(win->game);
}

/**
 * Saves the splits in the JSON Split file.
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void save_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    if (win->game && win->timer) {
        int width, height;
        gtk_window_get_default_size(GTK_WINDOW(win), &width, &height);
        win->game->width = width;
        win->game->height = height;
        bool save = true;
        if (cfg.libresplit.ask_on_worse.value.b) {
            if (!ls_is_timer_better(win->game, win->timer)) {
                save = false;
                const LSDialogOption options[] = {
                    {
                        .label = "_Yes",
                        .callback = perform_save_splits,
                        .is_cancel = FALSE,
                        .is_default = FALSE,
                    },
                    {
                        .label = "_No",
                        .callback = NULL,
                        .is_cancel = TRUE,
                        .is_default = TRUE,
                    }
                };

                const LSDialogIcon icon = {
                    .source = "dialog-question",
                    .type = LS_DIALOG_ICON_NAME,
                };

                ls_dialog_open(
                    GTK_WINDOW(win),
                    "LibreSplit",
                    "This run seems to be worse than the saved one. Continue?",
                    NULL,
                    &icon,
                    options,
                    G_N_ELEMENTS(options),
                    win,
                    NULL);
            }
        }

        if (save) {
            perform_save_splits(win);
        }
    }
}

/**
 * Reloads LibreSplit.
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void reload_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    LSAppWindow* win;
    char* path;
    if (parameter != NULL) {
        app = parameter;
    }

    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    if (win->game) {
        path = strdup(win->game->path);
        if (!path) {
            fprintf(stderr, "Out of memory duplicating path\n");
            return;
        }
        ls_app_window_open(win, path);
        free(path);
    }
}

/**
 * Closes the current split file, emptying the LibreSplit window.
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void close_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    timer_stop_and_reset(win);

    if (win->game && win->timer) {
        ls_app_window_clear_game(win);
    }
    if (win->timer) {
        ls_timer_release(win->timer);
        win->timer = 0;
    }
    if (win->game) {
        ls_game_release(win->game);
        win->game = 0;
    }
    gtk_widget_set_size_request(GTK_WIDGET(win), -1, -1);
}

/**
 * @brief Perform the quit operation after agreeable checks.
 *
 * @param window pointer to the main application window
 */
static void perform_quit(gpointer window)
{
    LSAppWindow* win = LS_APP_WINDOW(window);
    if (win->welcome_box) {
        welcome_box_destroy(win->welcome_box);
    }

    gtk_window_destroy(GTK_WINDOW(win));
}

/**
 * Exits LibreSplit.
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void quit_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    LOG_INFO("Exiting LibreSplit. GG!");
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    atomic_store(&exit_requested, 1);
    LOG_DEBUG("Exit request sent to threads");
    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    // Warn if the reset will lose a gold split, and allow the user to cancel the reset if they want to keep it
    if (win->timer && win->timer->running && (ls_timer_has_gold_split(win->timer) || ls_timer_has_rainbow_split(win->timer))) {
        if (cfg.libresplit.ask_on_gold.value.b) {
            display_confirm_reset_dialog(perform_quit, win);
            return;
        }
    }

    perform_quit(win);
}

/**
 * Callback to toggle the Auto Splitter on and off.
 *
 * @param action Pointer to the action that triggered this callback.
 * @param value The requested action state.
 * @param user_data Usually NULL
 */
void toggle_auto_splitter(GSimpleAction* action, GVariant* value, gpointer user_data)
{
    gboolean active = g_variant_get_boolean(value);
    atomic_store(&auto_splitter_enabled, active);
    cfg.libresplit.auto_splitter_enabled.value.b = active;
    config_save();
    g_simple_action_set_state(action, value);
}

/**
 * Callback to toggle the EWMH "Always on top" hint.
 *
 * @param action Pointer to the action that triggered this callback.
 * @param value The requested action state.
 * @param app Usually NULL
 */
void menu_toggle_win_on_top(GSimpleAction* action, GVariant* value, gpointer app)
{
    gboolean active = g_variant_get_boolean(value);
    LSAppWindow* win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    x11_set_keep_above(GTK_WINDOW(win), active);
    win->opts.win_on_top = active;
    g_simple_action_set_state(action, value);
}

/**
 * @brief Callback function for when a valid local file is selected as the new auto splitter.
 * It is safe to assume that parent is the LSAppWindow becuase the LSAppWindow is passed to
 * ls_file_picker_open from the implementer. This assumption must not change.
 *
 * @param parent The main app window
 * @param filename The successfully selected local file
 */
static void open_autosplitter_selected(GtkWindow* parent, const char* filename)
{
    LSAppWindow* win = LS_APP_WINDOW(parent);
    // Timer started while picking file sanity check
    if (win->timer && win->timer->running) {
        ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing the auto splitter.");
    } else {
        const char* folder_path = g_path_get_dirname(filename);
        CFG_SET_STR(cfg.history.last_auto_splitter_folder.value.s, folder_path);
        CFG_SET_STR(cfg.history.auto_splitter_file.value.s, filename);
        strcpy(auto_splitter_file, filename);
        config_save();

        // Restart auto-splitter if it was running
        restart_auto_splitter();

        g_free(folder_path);
    }
}

/**
 * Shows the "Open Lua Auto Splitter" dialog eventually using
 * the last known auto splitter folder. Also saves a new
 * "last used auto splitter folder".
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
void open_auto_splitter(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    char auto_splitters_path[PATH_MAX];
    LSAppWindow* win;
    struct stat st = { 0 };

    // Load the last used auto splitter folder, if present
    const char* last_auto_splitter_folder = cfg.history.last_auto_splitter_folder.value.s;
    if (parameter != NULL) {
        app = parameter;
    }

    win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
        win = ls_app_window_new(LS_APP(app));
    }

    if (win->timer && win->timer->running) {
        ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing the auto splitter.");
        return;
    }

    bool use_default_path = true;
    if (last_auto_splitter_folder != NULL && last_auto_splitter_folder[0] != '\0') {
        strcpy(auto_splitters_path, last_auto_splitter_folder);

        // Just use the last saved path if it exists
        if (stat(last_auto_splitter_folder, &st) == 0) {
            use_default_path = false;
        }
    }

    if (use_default_path) {
        strcpy(auto_splitters_path, win->data_path);
        strcat(auto_splitters_path, "/auto-splitters");
        if (stat(auto_splitters_path, &st) == -1) {
            mkdir(auto_splitters_path, 0700);
        }
    }

    const LSFilePickerFilter filters[] = { { .name = "LibreSplit LUA Auto Splitters", .pattern = "*.lua", .is_default = true } };
    const LSFilePickerOptions options = {
        .title = "Open Auto Splitter File",
        .path = auto_splitters_path,
        .filters = filters,
        .filters_count = G_N_ELEMENTS(filters),
    };

    // win here must ALWAYS be the LSAppWindow
    ls_file_picker_open(GTK_WINDOW(win), &options, open_autosplitter_selected);
}
