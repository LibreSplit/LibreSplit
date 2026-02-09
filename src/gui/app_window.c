#include "app_window.h"
#include "src/gui/actions.h"
#include "src/gui/component/components.h"
#include "src/gui/context_menu.h"
#include "src/gui/game.h"
#include "src/gui/theming.h"
#include "src/gui/timer.h"
#include "src/keybinds/delayed_callbacks.h"
#include "src/keybinds/keybinds_callbacks.h"
#include "src/lasr/auto-splitter.h"
#include "src/settings/settings.h"
#include "src/settings/utils.h"
#include <sys/stat.h>

extern atomic_bool exit_requested; /*!< Set to 1 when LibreSplit is exiting */

static void ls_app_init(LSApp* app)
{
}

G_DEFINE_TYPE(LSApp, ls_app, GTK_TYPE_APPLICATION)

G_DEFINE_TYPE(LSAppWindow, ls_app_window, GTK_TYPE_APPLICATION_WINDOW)

void toggle_decorations(LSAppWindow* win)
{
    gtk_window_set_decorated(GTK_WINDOW(win), !win->opts.decorated);
    win->opts.decorated = !win->opts.decorated;
    cfg.libresplit.start_decorated.value.b = win->opts.decorated;
    config_save();
}

void toggle_win_on_top(LSAppWindow* win)
{
    gtk_window_set_keep_above(GTK_WINDOW(win), !win->opts.win_on_top);
    win->opts.win_on_top = !win->opts.win_on_top;
    cfg.libresplit.start_on_top.value.b = win->opts.win_on_top;
    config_save();
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

gboolean ls_app_window_resize(GtkWidget* widget,
    GdkEvent* event,
    gpointer data)
{
    LSAppWindow* win = (LSAppWindow*)widget;
    resize_window(win, event->configure.width, event->configure.height);
    return FALSE;
}

LSAppWindow* ls_app_window_new(LSApp* app)
{
    LSAppWindow* win;
    win = g_object_new(LS_APP_WINDOW_TYPE, "application", app, NULL);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
    return win;
}

void ls_app_window_open(LSAppWindow* win, const char* file)
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
 * Starts LibreSplit, loading the last splits and auto splitter.
 * Eventually opens some dialogs if there are no last splits or auto-splitters.
 *
 * @param app Pointer to the LibreSplit application.
 */
void ls_app_activate(GApplication* app)
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

void ls_app_open(GApplication* app,
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

static void ls_app_window_class_init(LSAppWindowClass* class)
{
}

/**
 * Closes LibreSplit.
 *
 * @param widget The pointer to the LibreSplit window, as a widget.
 * @param data Usually NULL.
 */
void ls_app_window_destroy(GtkWidget* widget, gpointer data)
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
    // Close any other open application windows (settings, dialogs, etc.)
    GApplication* app = g_application_get_default();
    if (app) {
        GList* windows = gtk_application_get_windows(GTK_APPLICATION(app));
        GList* snapshot = g_list_copy(windows); // Copy to avoid race conditions
        for (GList* l = snapshot; l != NULL; l = l->next) {
            GtkWidget* w = GTK_WIDGET(l->data);
            if (w != GTK_WIDGET(win)) {
                gtk_widget_destroy(w);
            }
        }
        g_list_free(snapshot);
    }
}

/**
 * Updates the internal state of the LibreSplit Window.
 *
 * @param data Pointer to the LibreSplit Window.
 */
gboolean ls_app_window_step(gpointer data)
{
    LSAppWindow* win = data;
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
        ls_timer_step(win->timer);

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

                if (win->timer->running) {
                    if (win->timer->loading) {
                        timer_pause(win);
                    } else {
                        timer_unpause(win);
                    }
                }
                atomic_store(&toggle_loading, 0);
            }
            if (atomic_load(&call_reset)) {
                timer_stop_and_reset(win);
                atomic_store(&run_started, false);
                atomic_store(&call_reset, 0);
            }
            if (atomic_load(&update_game_time)) {
                // Update the timer with the game time from auto-splitter
                win->timer->gameTime = atomic_load(&game_time_value);
                atomic_store(&update_game_time, false);
            }
        }
    }
    process_delayed_handlers(win);

    return TRUE;
}

gboolean ls_app_window_draw(gpointer data)
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
    add_class(GTK_WIDGET(win), "main-window");
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
