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

static void open_splits_finished(GObject* diag, GAsyncResult* result, gpointer user_data)
{
    GtkFileDialog* dialog = GTK_FILE_DIALOG(diag);
    LSAppWindow* win = LS_APP_WINDOW(user_data);
    GError* error = NULL;
    GFile* file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file != NULL) {
        // Timer started while picking file - autosplitter/global hotkey start sanity check
        if (win->timer && win->timer->running) {
            ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing splits.");
        } else {
            char* filename = g_file_get_path(file);
            if (filename != NULL) {
                GFile* folder = g_file_get_parent(file);
                char* folder_path = folder != NULL ? g_file_get_path(folder) : NULL;

                if (folder_path != NULL) {
                    CFG_SET_STR(cfg.history.last_split_folder.value.s, folder_path);
                }

                ls_app_window_open(win, filename);
                CFG_SET_STR(cfg.history.split_file.value.s, filename);

                g_free(folder_path);
                g_clear_object(&folder);
                g_free(filename);
            } else {
                LOG_WARN("Selected split file did not have a local path");
            }
        }
    } else if (error != NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) && g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
            LOG_WARNF("Failed to open split file: %s", error->message);
        }
    } else {
        LOG_WARN("Split file open dialog returned no file");
    }

    if (!win->game || !win->timer) {
        gtk_widget_set_visible(win->welcome_box->box, TRUE);
    }

    config_save();
    g_clear_object(&file);
    g_clear_error(&error);
    g_object_unref(win);
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
    GtkFileDialog* dialog;
    GtkFileFilter* filter;
    GListStore* filters;
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

    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Splits File");

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_filter_set_name(filter, "LibreSplit JSON Split Files");
    filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    if (last_split_folder != NULL && last_split_folder[0] != '\0') {
        // Just use the last saved path
        strcpy(splits_path, last_split_folder);
    } else {
        // We have no saved path, go to the default splits path and eventually create it
        strcpy(splits_path, win->data_path);
        strcat(splits_path, "/splits");
        if (stat(splits_path, &st) == -1) {
            mkdir(splits_path, 0700);
        }
    }

    // We couldn't recover any previous split, open the file dialog
    GFile* folder = g_file_new_for_path(splits_path);
    gtk_file_dialog_set_initial_folder(dialog, folder);
    g_object_unref(folder);

    gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, open_splits_finished, g_object_ref(win));
    g_object_unref(dialog);
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
 * @param app pointer to the main application
 */
static void perform_quit(gpointer app)
{
    LSAppWindow* win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (win->welcome_box) {
        welcome_box_destroy(win->welcome_box);
    }

    gtk_window_destroy(GTK_WINDOW(win));
    g_application_quit(app);
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
            display_confirm_reset_dialog(perform_quit, NULL, NULL);
            return;
        }
    }

    perform_quit(app);
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

static void open_autosplitter_finished(GObject* diag, GAsyncResult* result, gpointer user_data)
{
    GtkFileDialog* dialog = GTK_FILE_DIALOG(diag);
    LSAppWindow* win = LS_APP_WINDOW(user_data);
    GError* error = NULL;
    GFile* file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file != NULL) {
        // Timer started while picking file - autosplitter/global hotkey start sanity check
        if (win->timer && win->timer->running) {
            ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing auto splitter.");
        } else {
            char* filename = g_file_get_path(file);
            if (filename != NULL) {
                GFile* folder = g_file_get_parent(file);
                char* folder_path = folder != NULL ? g_file_get_path(folder) : NULL;

                if (folder_path != NULL) {
                    CFG_SET_STR(cfg.history.last_auto_splitter_folder.value.s, folder_path);
                }

                CFG_SET_STR(cfg.history.auto_splitter_file.value.s, filename);
                strcpy(auto_splitter_file, filename);
                config_save();

                // Restart auto-splitter if it was running
                restart_auto_splitter();

                g_free(folder_path);
                g_clear_object(&folder);
                g_free(filename);
            } else {
                LOG_WARN("Selected auto splitter did not have a local path");
            }
        }
    } else if (error != NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) && g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
            LOG_WARNF("Failed to open auto splitter: %s", error->message);
        }
    } else {
        LOG_WARN("Auto Splitter file open dialog returned no file");
    }

    g_clear_object(&file);
    g_clear_error(&error);
    g_object_unref(win);
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
    GtkFileDialog* dialog;
    GtkFileFilter* filter;
    GListStore* filters;
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
        ls_alert_info(GTK_WINDOW(win), "LibreSplit", "The timer is currently running", "Please stop the run before changing auto splitter.");
        return;
    }

    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Auto Splitter File");
    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.lua");
    gtk_file_filter_set_name(filter, "LibreSplit Lua Auto Splitters");
    filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    if (last_auto_splitter_folder != NULL && last_auto_splitter_folder[0] != '\0') {
        // Just use the last saved path
        strcpy(auto_splitters_path, last_auto_splitter_folder);
    } else {
        strcpy(auto_splitters_path, win->data_path);
        strcat(auto_splitters_path, "/auto-splitters");
        if (stat(auto_splitters_path, &st) == -1) {
            mkdir(auto_splitters_path, 0700);
        }
    }

    GFile* folder = g_file_new_for_path(auto_splitters_path);
    gtk_file_dialog_set_initial_folder(dialog, folder);
    g_object_unref(folder);

    gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, open_autosplitter_finished, g_object_ref(win));
    g_object_unref(dialog);
}
