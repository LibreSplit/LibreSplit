#include "src/gui/actions.h"
#include "src/gui/app_window.h"
#include "src/gui/game.h"
#include "src/gui/timer.h"
#include "src/lasr/auto-splitter.h"
#include "src/lasr/utils.h"
#include "src/settings/settings.h"
#include <gtk/gtk.h>
#include <sys/stat.h>

/**
 * Compares the current timer and the saved one to see
 * if the current one is better
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
    // Find the latest split with a time
    for (i = game->split_count - 1; i >= 0; i--) {
        if (timer->split_times[i] != 0ll || game->split_times[i] != 0ll) {
            break;
        }
    }
    if (i < 0) {
        return true;
    }
    if (timer->split_times[i] == 0ll) {
        return false;
    }
    if (game->split_times[i] == 0ll) {
        return true;
    }
    return timer->split_times[i] <= game->split_times[i];
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
    GList* windows;
    LSAppWindow* win;
    GtkWidget* dialog;
    GtkFileFilter* filter;
    struct stat st = { 0 };
    gint res;
    // Load the last used split folder, if present
    const char* last_split_folder = cfg.history.last_split_folder.value.s;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    if (is_run_started(win->timer)) {
        GtkWidget* warning = gtk_message_dialog_new(
            GTK_WINDOW(win),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "The timer is currently running, please stop the run before changing splits.");
        gtk_dialog_run(GTK_DIALOG(warning));
        gtk_widget_destroy(warning);
        return;
    }
    dialog = gtk_file_chooser_dialog_new(
        "Open File", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_filter_set_name(filter, "LibreSplit JSON Split Files");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (last_split_folder != NULL) {
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
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
        splits_path);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char* filename;
        GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
        char last_folder[PATH_MAX];
        filename = gtk_file_chooser_get_filename(chooser);
        strcpy(last_folder, gtk_file_chooser_get_current_folder(chooser));
        CFG_SET_STR(cfg.history.last_split_folder.value.s, last_folder);
        ls_app_window_open(win, filename);
        CFG_SET_STR(cfg.history.split_file.value.s, filename);
        g_free(filename);
    }
    if (!win->game || !win->timer) {
        gtk_widget_show_all(win->welcome_box->box);
    }
    gtk_widget_destroy(dialog);
    config_save();
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
    GList* windows;
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    if (win->game && win->timer) {
        int width, height;
        gtk_window_get_size(GTK_WINDOW(win), &width, &height);
        win->game->width = width;
        win->game->height = height;
        bool saving = true;
        if (cfg.libresplit.ask_on_worse.value.b) {
            if (!ls_is_timer_better(win->game, win->timer)) {
                GtkWidget* confirm = gtk_message_dialog_new(
                    GTK_WINDOW(win),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_QUESTION,
                    GTK_BUTTONS_YES_NO,
                    "This run seems to be worse than the saved one. Continue?");
                gint response = gtk_dialog_run(GTK_DIALOG(confirm));
                if (response == GTK_RESPONSE_NO) {
                    saving = false;
                }
                gtk_widget_destroy(confirm);
            }
        }
        if (saving) {
            ls_game_update_splits(win->game, win->timer);
            save_game(win->game);
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
    GList* windows;
    LSAppWindow* win;
    char* path;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
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
    GList* windows;
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
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
    GList* windows;
    LSAppWindow* win;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    if (win->welcome_box) {
        welcome_box_destroy(win->welcome_box);
    }
    exit(0);
}

/**
 * Callback to toggle the Auto Splitter on and off.
 *
 * @param menu_item Pointer to the menu item that triggered this callback.
 * @param user_data Usually NULL
 */
void toggle_auto_splitter(GtkCheckMenuItem* menu_item, gpointer user_data)
{
    gboolean active = gtk_check_menu_item_get_active(menu_item);
    atomic_store(&auto_splitter_enabled, active);
    cfg.libresplit.auto_splitter_enabled.value.b = active;
    config_save();
}

/**
 * Callback to toggle the EWMH "Always on top" hint.
 *
 * @param menu_item Pointer to the menu item that triggered this callback.
 * @param app Usually NULL
 */
void menu_toggle_win_on_top(GtkCheckMenuItem* menu_item,
    gpointer app)
{
    gboolean active = gtk_check_menu_item_get_active(menu_item);
    GList* windows;
    LSAppWindow* win;
    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    gtk_window_set_keep_above(GTK_WINDOW(win), !win->opts.win_on_top);
    win->opts.win_on_top = active;
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
    GList* windows;
    LSAppWindow* win;
    GtkWidget* dialog;
    GtkFileFilter* filter;
    struct stat st = { 0 };
    gint res;
    // Load the last used auto splitter folder, if present
    const char* last_auto_splitter_folder = cfg.history.last_auto_splitter_folder.value.s;
    if (parameter != NULL) {
        app = parameter;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    if (is_run_started(win->timer)) {
        GtkWidget* warning = gtk_message_dialog_new(
            GTK_WINDOW(win),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "The timer is currently running, please stop the run before changing auto splitter.");
        gtk_dialog_run(GTK_DIALOG(warning));
        gtk_widget_destroy(warning);
        return;
    }
    dialog = gtk_file_chooser_dialog_new(
        "Open File", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.lua");
    gtk_file_filter_set_name(filter, "LibreSplit Lua Auto Splitters");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (last_auto_splitter_folder != NULL) {
        // Just use the last saved path
        strcpy(auto_splitters_path, last_auto_splitter_folder);
    } else {
        strcpy(auto_splitters_path, win->data_path);
        strcat(auto_splitters_path, "/auto-splitters");
        if (stat(auto_splitters_path, &st) == -1) {
            mkdir(auto_splitters_path, 0700);
        }
    }
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
        auto_splitters_path);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
        char* filename = gtk_file_chooser_get_filename(chooser);
        char last_folder[PATH_MAX];
        strcpy(last_folder, gtk_file_chooser_get_current_folder(chooser));
        CFG_SET_STR(cfg.history.last_auto_splitter_folder.value.s, last_folder);
        CFG_SET_STR(cfg.history.auto_splitter_file.value.s, filename);
        strcpy(auto_splitter_file, filename);
        config_save();

        // Restart auto-splitter if it was running
        restart_auto_splitter();

        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}
