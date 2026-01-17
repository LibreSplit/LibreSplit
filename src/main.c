#include "gui/app_window.h"
#include "gui/component/components.h"
#include "gui/utils.h"
#include "gui/welcome_box.h"
#include "keybinds/keybinds.h"
#include "keybinds/keybinds_callbacks.h"
#include "lasr/auto-splitter.h"
#include "server.h"
#include "settings/settings.h"
#include "settings/utils.h"
#include "shared.h"
#include "timer.h"

#include <gtk/gtk.h>
#include <jansson.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#define LS_APP_TYPE (ls_app_get_type())
#define LS_APP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_TYPE, LSApp))

G_DEFINE_TYPE(LSApp, ls_app, GTK_TYPE_APPLICATION)

#define LS_APP_WINDOW_TYPE (ls_app_window_get_type())
#define LS_APP_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_WINDOW_TYPE, LSAppWindow))

G_DEFINE_TYPE(LSAppWindow, ls_app_window, GTK_TYPE_APPLICATION_WINDOW)

#define WINDOW_PAD (8)

atomic_bool exit_requested = 0; /*!< Set to 1 when LibreSplit is exiting */

static const unsigned char fallback_css_data[] = {
#embed "fallback.css"
};

static const size_t fallback_css_data_len = sizeof(fallback_css_data);

/**
 * Closes LibreSplit.
 *
 * @param widget The pointer to the LibreSplit window, as a widget.
 * @param data Usually NULL.
 */
static void ls_app_window_destroy(GtkWidget* widget, gpointer data)
{
    LSAppWindow* win = (LSAppWindow*)widget;
    if (win->timer) {
        ls_timer_release(win->timer);
    }
    if (win->game) {
        ls_game_release(win->game);
    }
    atomic_store(&auto_splitter_enabled, 0);
    atomic_store(&exit_requested, 1);
}

static gpointer save_game_thread(gpointer data)
{
    ls_game* game = data;
    ls_game_save(game);
    return NULL;
}

static void save_game(ls_game* game)
{
    g_thread_new("save_game", save_game_thread, game);
}

// Forward declarations
static void timer_start(LSAppWindow* win, bool updateComponents);
static void timer_stop(LSAppWindow* win);
static void timer_split(LSAppWindow* win, bool updateComponents);
static void timer_reset(LSAppWindow* win);

/**
 * Updates the internal state of the LibreSplit Window.
 *
 * @param data Pointer to the LibreSplit Window.
 */
static gboolean ls_app_window_step(gpointer data)
{
    LSAppWindow* win = data;
    long long now = ls_time_now();
    static int set_cursor;
    if (win->opts.hide_cursor && !set_cursor) {
        GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(win));
        if (gdk_window) {
            GdkCursor* cursor = gdk_cursor_new_for_display(win->display, GDK_BLANK_CURSOR);
            gdk_window_set_cursor(gdk_window, cursor);
            set_cursor = 1;
        }
    }
    if (win->timer) {
        ls_timer_step(win->timer, now);

        if (atomic_load(&auto_splitter_enabled)) {
            if (atomic_load(&call_start) && !win->timer->loading) {
                timer_start(win, true);
                atomic_store(&call_start, 0);
            }
            if (atomic_load(&call_split)) {
                timer_split(win, true);
                atomic_store(&call_split, 0);
            }
            if (atomic_load(&toggle_loading)) {
                win->timer->loading = !win->timer->loading;
                if (win->timer->running && win->timer->loading) {
                    timer_stop(win);
                } else if (win->timer->started && !win->timer->running && !win->timer->loading) {
                    timer_start(win, true);
                }
                atomic_store(&toggle_loading, 0);
            }
            if (atomic_load(&call_reset)) {
                timer_reset(win);
                atomic_store(&run_started, false);
                atomic_store(&call_reset, 0);
            }
            if (atomic_load(&update_game_time)) {
                // Update the timer with the game time from auto-splitter
                win->timer->time = atomic_load(&game_time_value);
                atomic_store(&update_game_time, false);
            }
        }
    }

    return TRUE;
}

/**
 * Finds a theme, given its name and variant.
 *
 * @param win The LibreSplit Window.
 * @param name The name of the theme to load.
 * @param variant The name of the variant to load (can be empty).
 * @param out_path Pointer to a string onto which the theme path will be copied.
 *
 * @return 1 if the load is successful, 0 otherwise.
 */
static int ls_app_window_find_theme(const LSAppWindow* win,
    const char* name,
    const char* variant,
    char* out_path)
{
    if (!name || !strlen(name)) {
        out_path[0] = '\0';
        return 0;
    }

    char theme_path[PATH_MAX];
    strcpy(theme_path, "/");
    strcat(theme_path, name);
    strcat(theme_path, "/");
    strcat(theme_path, name);
    if (variant && strlen(variant)) {
        strcat(theme_path, "-");
        strcat(theme_path, variant);
    }
    strcat(theme_path, ".css");

    strcpy(out_path, win->data_path);
    strcat(out_path, "/themes");
    strcat(out_path, theme_path);
    struct stat st = { 0 };
    if (stat(out_path, &st) == -1) {
        return 0;
    }
    return 1;
}

/**
 * Loads a specific theme, with a fallback to the default theme
 *
 * @param win The LibreSplit window.
 * @param name The name of the theme to load.
 * @param variant The variant of the theme to load.
 */
static void ls_app_load_theme_with_fallback(LSAppWindow* win, const char* name, const char* variant)
{
    char path[PATH_MAX];

    // Remove old style
    if (win->style) {
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(win->style));
        g_object_unref(win->style);
        win->style = nullptr;
    }

    if (!win->style) {
        win->style = gtk_css_provider_new();
    }

    GError* gerror = nullptr;

    const bool found = ls_app_window_find_theme(win, name, variant, path);
    bool error = false;

    if (!found) {
        printf("Theme not found: \"%s\" (variant: \"%s\")\n", name ? name : "", variant ? variant : "");
    }

    if (found) {
        GdkScreen* screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(
            GTK_CSS_PROVIDER(win->style),
            path, &gerror);
        if (gerror != nullptr) {
            g_printerr("Error loading custom theme CSS: %s\n", gerror->message);
            error = true;
            g_error_free(gerror);
            gerror = nullptr;
        }
    }

    if (!found || error) {
        // Load default theme from embedded CSS as fallback
        GdkScreen* screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_data(
            GTK_CSS_PROVIDER(win->style),
            (const char*)fallback_css_data,
            fallback_css_data_len, &gerror);
        if (gerror != nullptr) {
            g_printerr("Error loading default theme CSS: %s\n", gerror->message);
            error = true;
            g_error_free(gerror);
            gerror = nullptr;
        }
    }
}

/**
 * Clears the current game and reset all the components.
 *
 * @param win The LibreSplit app window
 */
static void ls_app_window_clear_game(LSAppWindow* win)
{
    GList* l;

    atomic_store(&run_finished, false);

    gtk_widget_hide(win->box);
    gtk_widget_show_all(win->welcome_box->box);

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
static void ls_app_window_show_game(LSAppWindow* win)
{
    GList* l;

    // set dimensions
    if (win->game->width > 0 && win->game->height > 0) {
        gtk_widget_set_size_request(GTK_WIDGET(win),
            win->game->width,
            win->game->height);
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

    gtk_widget_show(win->box);
    gtk_widget_hide(win->welcome_box->box);
}

static void resize_window(LSAppWindow* win,
    int window_width,
    int window_height)
{
    GList* l;
    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->resize) {
            component->ops->resize(component,
                window_width - 2 * WINDOW_PAD,
                window_height);
        }
    }
}

static gboolean ls_app_window_resize(GtkWidget* widget,
    GdkEvent* event,
    gpointer data)
{
    LSAppWindow* win = (LSAppWindow*)widget;
    resize_window(win, event->configure.width, event->configure.height);
    return FALSE;
}

void timer_start_split(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (!win->timer->running) {
            if (ls_timer_start(win->timer)) {
                save_game(win->game);
            }
        } else {
            timer_split(win, false);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->start_split) {
                component->ops->start_split(component, win->timer);
            }
        }
    }
}

static void timer_start(LSAppWindow* win, bool updateComponents)
{
    if (win->timer) {
        GList* l;
        if (!win->timer->running) {
            if (ls_timer_start(win->timer)) {
                save_game(win->game);
            }
            if (updateComponents) {
                for (l = win->components; l != NULL; l = l->next) {
                    LSComponent* component = l->data;
                    if (component->ops->start_split) {
                        component->ops->start_split(component, win->timer);
                    }
                }
            }
        }
    }
}

static void timer_split(LSAppWindow* win, bool updateComponents)
{
    if (win->timer) {
        GList* l;
        ls_timer_split(win->timer);
        if (updateComponents) {
            for (l = win->components; l != NULL; l = l->next) {
                LSComponent* component = l->data;
                if (component->ops->start_split) {
                    component->ops->start_split(component, win->timer);
                }
            }
        }
    }
}

static void timer_stop(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (win->timer->running) {
            ls_timer_stop(win->timer);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}

void timer_stop_reset(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (atomic_load(&run_started) || win->timer->running) {
            ls_timer_stop(win->timer);
        } else {
            const bool was_asl_enabled = atomic_load(&auto_splitter_enabled);
            atomic_store(&auto_splitter_enabled, false);
            while (atomic_load(&auto_splitter_running) && was_asl_enabled) {
                // wait, this will be very fast so its ok to just spin
            }
            if (was_asl_enabled)
                atomic_store(&auto_splitter_enabled, true);

            if (ls_timer_reset(win->timer)) {
                ls_app_window_clear_game(win);
                ls_app_window_show_game(win);
                save_game(win->game);
            }
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}

static void timer_reset(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (win->timer->running) {
            ls_timer_stop(win->timer);
            for (l = win->components; l != NULL; l = l->next) {
                LSComponent* component = l->data;
                if (component->ops->stop_reset) {
                    component->ops->stop_reset(component, win->timer);
                }
            }
        }
        if (ls_timer_reset(win->timer)) {
            ls_app_window_clear_game(win);
            ls_app_window_show_game(win);
            save_game(win->game);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}

void timer_cancel_run(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (ls_timer_cancel(win->timer)) {
            ls_app_window_clear_game(win);
            ls_app_window_show_game(win);
            save_game(win->game);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->cancel_run) {
                component->ops->cancel_run(component, win->timer);
            }
        }
    }
}

void timer_skip(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        ls_timer_skip(win->timer);
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->skip) {
                component->ops->skip(component, win->timer);
            }
        }
    }
}

void timer_unsplit(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        ls_timer_unsplit(win->timer);
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->unsplit) {
                component->ops->unsplit(component, win->timer);
            }
        }
    }
}

void toggle_decorations(LSAppWindow* win)
{
    gtk_window_set_decorated(GTK_WINDOW(win), !win->opts.decorated);
    win->opts.decorated = !win->opts.decorated;
}

void toggle_win_on_top(LSAppWindow* win)
{
    gtk_window_set_keep_above(GTK_WINDOW(win), !win->opts.win_on_top);
    win->opts.win_on_top = !win->opts.win_on_top;
}

// Global application instance for CTL command handling
static LSApp* g_app = NULL;

// Function to handle CTL commands from the server thread
void handle_ctl_command(CTLCommand command)
{
    GList* windows;
    LSAppWindow* win;

    if (!g_app) {
        printf("No application instance available to handle command\n");
        return;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(g_app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        printf("No window available to handle command\n");
        return;
    }

    switch (command) {
        case CTL_CMD_START_SPLIT:
            timer_start_split(win);
            break;
        case CTL_CMD_STOP_RESET:
            timer_stop_reset(win);
            break;
        case CTL_CMD_CANCEL:
            timer_cancel_run(win);
            break;
        case CTL_CMD_UNSPLIT:
            timer_unsplit(win);
            break;
        case CTL_CMD_SKIP:
            timer_skip(win);
            break;
        case CTL_CMD_EXIT:
            exit(0);
            break;
        default:
            printf("Unknown CTL command: %d\n", command);
            break;
    }
}

static gboolean ls_app_window_draw(gpointer data)
{
    LSAppWindow* win = data;
    if (win->timer) {
        GList* l;
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->draw) {
                component->ops->draw(component, win->game, win->timer);
            }
        }
    } else {
        GdkRectangle rect;
        gtk_widget_get_allocation(GTK_WIDGET(win), &rect);
        gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(win)),
            &rect, FALSE);
    }
    return TRUE;
}

static void ls_app_window_init(LSAppWindow* win)
{
    const char* theme;
    const char* theme_variant;
    int i;

    win->display = gdk_display_get_default();
    win->style = NULL;

    // make data path
    win->data_path[0] = '\0';
    get_libresplit_folder_path(win->data_path);

    // load settings
    win->opts.hide_cursor = cfg.libresplit.hide_cursor.value.b;
    win->opts.global_hotkeys = cfg.libresplit.global_hotkeys.value.b;
    win->opts.decorated = cfg.libresplit.start_decorated.value.b;
    win->opts.win_on_top = cfg.libresplit.start_on_top.value.b;
    win->keybinds.start_split = parse_keybind(cfg.keybinds.start_split.value.s);
    win->keybinds.stop_reset = parse_keybind(cfg.keybinds.stop_reset.value.s);
    win->keybinds.cancel = parse_keybind(cfg.keybinds.cancel.value.s);
    win->keybinds.unsplit = parse_keybind(cfg.keybinds.unsplit.value.s);
    win->keybinds.skip_split = parse_keybind(cfg.keybinds.skip_split.value.s);
    win->keybinds.toggle_decorations = parse_keybind(cfg.keybinds.toggle_decorations.value.s);
    win->keybinds.toggle_win_on_top = parse_keybind(cfg.keybinds.toggle_win_on_top.value.s);
    gtk_window_set_decorated(GTK_WINDOW(win), win->opts.decorated);
    gtk_window_set_keep_above(GTK_WINDOW(win), win->opts.win_on_top);

    // Load theme
    theme = cfg.libresplit.theme.value.s;
    theme_variant = cfg.libresplit.theme_variant.value.s;
    ls_app_load_theme_with_fallback(win, theme, theme_variant);

    // Load window junk
    add_class(GTK_WIDGET(win), "window");
    win->game = 0;
    win->timer = 0;

    g_signal_connect(win, "destroy",
        G_CALLBACK(ls_app_window_destroy), NULL);
    g_signal_connect(win, "configure-event",
        G_CALLBACK(ls_app_window_resize), win);

    // As a crash workaround, only enable global hotkeys if not on Wayland
    const bool is_wayland = getenv("WAYLAND_DISPLAY");
    const bool force_global_hotkeys = getenv("LIBRESPLIT_FORCE_GLOBAL_HOTKEYS");

    const bool enable_global_hotkeys = win->opts.global_hotkeys && (force_global_hotkeys || !is_wayland);

    if (enable_global_hotkeys) {
        bind_global_hotkeys(cfg, win);
    } else {
        g_signal_connect(win, "key_press_event",
            G_CALLBACK(ls_app_window_keypress), win);
    }

    win->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(win->container, WINDOW_PAD);
    gtk_widget_set_margin_bottom(win->container, WINDOW_PAD);
    gtk_widget_set_vexpand(win->container, TRUE);
    gtk_container_add(GTK_CONTAINER(win), win->container);
    gtk_widget_show(win->container);

    win->welcome_box = welcome_box_new(win->container);

    win->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(win->welcome_box->box, "main-screen");
    gtk_widget_set_margin_top(win->box, 0);
    gtk_widget_set_margin_bottom(win->box, 0);
    gtk_widget_set_vexpand(win->box, TRUE);
    gtk_container_add(GTK_CONTAINER(win->container), win->box);

    // Create all available components (TODO: change this in the future)
    win->components = NULL;
    for (i = 0; ls_components[i].name != NULL; i++) {
        LSComponent* component = ls_components[i].new();
        if (component) {
            GtkWidget* widget = component->ops->widget(component);
            if (widget) {
                gtk_widget_set_margin_start(widget, WINDOW_PAD);
                gtk_widget_set_margin_end(widget, WINDOW_PAD);
                gtk_container_add(GTK_CONTAINER(win->box),
                    component->ops->widget(component));
            }
            win->components = g_list_append(win->components, component);
        }
    }

    // NOTE: This always creates an empty footer, no matter how many
    //  ^ "footers" are available, which may give issues with theming
    win->footer = gtk_grid_new();
    add_class(win->footer, "footer");
    gtk_widget_set_margin_start(win->footer, WINDOW_PAD);
    gtk_widget_set_margin_end(win->footer, WINDOW_PAD);
    gtk_container_add(GTK_CONTAINER(win->box), win->footer);
    gtk_widget_show(win->footer);

    // Update the internal state every millisecond
    g_timeout_add(1, ls_app_window_step, win);
    // Draw the window at 30 FPS
    g_timeout_add((int)(1000 / 30.), ls_app_window_draw, win);
}

static void ls_app_window_class_init(LSAppWindowClass* class)
{
}

static LSAppWindow* ls_app_window_new(LSApp* app)
{
    LSAppWindow* win;
    win = g_object_new(LS_APP_WINDOW_TYPE, "application", app, NULL);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
    return win;
}

static void ls_app_window_open(LSAppWindow* win, const char* file)
{
    char* error_msg = NULL;
    GtkWidget* error_popup;

    if (win->timer) {
        ls_app_window_clear_game(win);
        ls_timer_release(win->timer);
        win->timer = 0;
    }
    if (win->game) {
        ls_game_release(win->game);
        win->game = 0;
    }
    if (ls_game_create(&win->game, file, &error_msg)) {
        win->game = 0;
        if (error_msg) {
            error_popup = gtk_message_dialog_new(
                GTK_WINDOW(win),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "JSON parse error: %s\n%s",
                error_msg,
                file);
            gtk_dialog_run(GTK_DIALOG(error_popup));

            free(error_msg);
            gtk_widget_destroy(error_popup);
        }
    } else if (ls_timer_create(&win->timer, win->game)) {
        win->timer = 0;
    } else {
        ls_app_window_show_game(win);
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
static void open_activated(GSimpleAction* action,
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
    } else {
        gtk_widget_show_all(win->welcome_box->box);
    }
    gtk_widget_destroy(dialog);
    config_save();
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
static void open_auto_splitter(GSimpleAction* action,
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
        const bool was_asl_enabled = atomic_load(&auto_splitter_enabled);
        if (was_asl_enabled) {
            atomic_store(&auto_splitter_enabled, false);
            while (atomic_load(&auto_splitter_running) && was_asl_enabled) {
                // wait, this will be very fast so its ok to just spin
            }
            if (was_asl_enabled)
                atomic_store(&auto_splitter_enabled, true);
        }

        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

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
    return timer->split_times[i] < game->split_times[i];
}

/**
 * Saves the splits in the JSON Split file.
 *
 * @param action Usually NULL
 * @param parameter Usually NULL
 * @param app Pointer to the LibreSplit app.
 */
static void save_activated(GSimpleAction* action,
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
        if (!ls_is_timer_better(win->game, win->timer)) {
            GtkWidget* confirm = gtk_message_dialog_new(
                GTK_WINDOW(win),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_QUESTION,
                GTK_BUTTONS_YES_NO,
                "This run seems to be worse than the saved one. Continue?");
            gint response = gtk_dialog_run(GTK_DIALOG(confirm));
            if (!response) {
                saving = false;
            }
            gtk_widget_destroy(confirm);
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
static void reload_activated(GSimpleAction* action,
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
static void close_activated(GSimpleAction* action,
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
    if (win->welcome_box) {
        welcome_box_destroy(win->welcome_box);
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
static void quit_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    exit(0);
}

/**
 * Callback to toggle the Auto Splitter on and off.
 *
 * @param menu_item Pointer to the menu item that triggered this callback.
 * @param user_data Usually NULL
 */
static void toggle_auto_splitter(GtkCheckMenuItem* menu_item, gpointer user_data)
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
static void menu_toggle_win_on_top(GtkCheckMenuItem* menu_item,
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
 * Creates the Context Menu.
 *
 * @param widget The widget that was right clicked. Not used here.
 * @param event The click event, containing which button was used to click.
 * @param app Pointer to the LibreSplit application.
 *
 * @return True if the click was done with the RMB (and a context menu was shown), False otherwise.
 */
static gboolean button_right_click(GtkWidget* widget, GdkEventButton* event, gpointer app)
{
    if (event->button == GDK_BUTTON_SECONDARY) {
        GList* windows;
        LSAppWindow* win;
        windows = gtk_application_get_windows(GTK_APPLICATION(app));
        if (windows) {
            win = LS_APP_WINDOW(windows->data);
        } else {
            win = ls_app_window_new(LS_APP(app));
        }
        GtkWidget* menu = gtk_menu_new();
        GtkWidget* menu_open_splits = gtk_menu_item_new_with_label("Open Splits");
        GtkWidget* menu_save_splits = gtk_menu_item_new_with_label("Save Splits");
        GtkWidget* menu_open_auto_splitter = gtk_menu_item_new_with_label("Open Auto Splitter");
        GtkWidget* menu_enable_auto_splitter = gtk_check_menu_item_new_with_label("Enable Auto Splitter");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_enable_auto_splitter), atomic_load(&auto_splitter_enabled));
        GtkWidget* menu_enable_win_on_top = gtk_check_menu_item_new_with_label("Always on Top");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_enable_win_on_top), win->opts.win_on_top);
        GtkWidget* menu_reload = gtk_menu_item_new_with_label("Reload");
        GtkWidget* menu_close = gtk_menu_item_new_with_label("Close");
        GtkWidget* menu_quit = gtk_menu_item_new_with_label("Quit");

        // Add the menu items to the menu
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_open_splits);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_save_splits);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_open_auto_splitter);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_enable_auto_splitter);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_enable_win_on_top);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_reload);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_close);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_quit);

        // Attach the callback functions to the menu items
        g_signal_connect(menu_open_splits, "activate", G_CALLBACK(open_activated), app);
        g_signal_connect(menu_save_splits, "activate", G_CALLBACK(save_activated), app);
        g_signal_connect(menu_open_auto_splitter, "activate", G_CALLBACK(open_auto_splitter), app);
        g_signal_connect(menu_enable_auto_splitter, "toggled", G_CALLBACK(toggle_auto_splitter), NULL);
        g_signal_connect(menu_enable_win_on_top, "toggled", G_CALLBACK(menu_toggle_win_on_top), app);
        g_signal_connect(menu_reload, "activate", G_CALLBACK(reload_activated), app);
        g_signal_connect(menu_close, "activate", G_CALLBACK(close_activated), app);
        g_signal_connect(menu_quit, "activate", G_CALLBACK(quit_activated), app);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    return FALSE;
}

/**
 * Starts LibreSplit, loading the last splits and auto splitter.
 * Eventually opens some dialogs if there are no last splits or auto-splitters.
 *
 * @param app Pointer to the LibreSplit application.
 */
static void ls_app_activate(GApplication* app)
{
    if (!config_init()) {
        printf("Configuration failed to load, will use defaults\n");
    }

    LSAppWindow* win;
    win = ls_app_window_new(LS_APP(app));
    gtk_window_present(GTK_WINDOW(win));

    if (cfg.history.split_file.value.s[0] != '\0') {
        // Check if split file exists
        struct stat st = { 0 };
        char splits_path[PATH_MAX];
        strcpy(splits_path, cfg.history.split_file.value.s);
        if (stat(splits_path, &st) == -1) {
            printf("Split JSON %s does not exist\n", splits_path);
            open_activated(NULL, NULL, app);
        } else {
            ls_app_window_open(win, splits_path);
        }
    } else {
        open_activated(NULL, NULL, app);
    }
    if (cfg.history.auto_splitter_file.value.s[0] != '\0') {
        struct stat st = { 0 };
        char auto_splitters_path[PATH_MAX];
        strcpy(auto_splitters_path, cfg.history.auto_splitter_file.value.s);
        if (stat(auto_splitters_path, &st) == -1) {
            printf("Auto Splitter %s does not exist\n", auto_splitters_path);
        } else {
            strcpy(auto_splitter_file, auto_splitters_path);
        }
    }
    atomic_store(&auto_splitter_enabled, cfg.libresplit.auto_splitter_enabled.value.b);
    g_signal_connect(win, "button_press_event", G_CALLBACK(button_right_click), app);
}

static void ls_app_init(LSApp* app)
{
}

static void ls_app_open(GApplication* app,
    GFile** files,
    gint n_files,
    const gchar* hint)
{
    GList* windows;
    LSAppWindow* win;
    int i;
    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        win = ls_app_window_new(LS_APP(app));
    }
    for (i = 0; i < n_files; i++) {
        ls_app_window_open(win, g_file_get_path(files[i]));
    }
    gtk_window_present(GTK_WINDOW(win));
}

LSApp* ls_app_new(void)
{
    g_set_application_name("LibreSplit");
    return g_object_new(LS_APP_TYPE,
        "application-id", "com.github.wins1ey.libresplit",
        "flags", G_APPLICATION_HANDLES_OPEN,
        NULL);
}

static void ls_app_class_init(LSAppClass* class)
{
    G_APPLICATION_CLASS(class)->activate = ls_app_activate;
    G_APPLICATION_CLASS(class)->open = ls_app_open;
}

/**
 * LibreSplit's auto splitter thread.
 *
 * @param arg Unused.
 */
static void* ls_auto_splitter(void* arg)
{
    prctl(PR_SET_NAME, "LS LASR", 0, 0, 0);
    while (1) {
        if (atomic_load(&auto_splitter_enabled) && auto_splitter_file[0] != '\0') {
            atomic_store(&auto_splitter_running, true);
            run_auto_splitter();
        }
        atomic_store(&auto_splitter_running, false);
        if (atomic_load(&exit_requested))
            return 0;
        usleep(50000);
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    check_directories();

    g_app = ls_app_new();
    pthread_t t1; // Auto-splitter thread
    pthread_create(&t1, NULL, &ls_auto_splitter, NULL);

    pthread_t t2; // Control server thread
    pthread_create(&t2, NULL, &ls_ctl_server, NULL);

    g_application_run(G_APPLICATION(g_app), argc, argv);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
