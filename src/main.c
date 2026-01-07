#include <jansson.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "auto-splitter.h"
#include "bind.h"
#include "component/components.h"
#include "server.h"
#include "settings.h"
#include "shared.h"
#include "timer.h"

#define LS_APP_TYPE (ls_app_get_type())
#define LS_APP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_TYPE, LSApp))

typedef struct _LSApp LSApp;
typedef struct _LSAppClass LSAppClass;

#define LS_APP_WINDOW_TYPE (ls_app_window_get_type())
#define LS_APP_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_WINDOW_TYPE, LSAppWindow))

typedef struct _LSAppWindow LSAppWindow;
typedef struct _LSAppWindowClass LSAppWindowClass;

#define WINDOW_PAD (8)

atomic_bool exit_requested = 0;

static const unsigned char css_data[] = {
#embed "main.css"
};

static const size_t css_data_len = sizeof(css_data);

typedef struct
{
    guint key;
    GdkModifierType mods;
} Keybind;

struct _LSAppWindow {
    GtkApplicationWindow parent;
    char data_path[PATH_MAX];
    gboolean decorated;
    gboolean win_on_top;
    ls_game* game;
    ls_timer* timer;
    GdkDisplay* display;
    GtkWidget* container;
    GtkWidget* welcome;
    GtkWidget* welcome_lbl;
    GtkWidget* box;
    GList* components;
    GtkWidget* footer;
    GtkCssProvider* style;
    gboolean hide_cursor;
    gboolean global_hotkeys;
    Keybind keybind_start_split;
    Keybind keybind_stop_reset;
    Keybind keybind_cancel;
    Keybind keybind_unsplit;
    Keybind keybind_skip_split;
    Keybind keybind_toggle_decorations;
    Keybind keybind_toggle_win_on_top;
};

struct _LSAppWindowClass {
    GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(LSAppWindow, ls_app_window, GTK_TYPE_APPLICATION_WINDOW)

static Keybind parse_keybind(const gchar* accelerator)
{
    Keybind kb;
    gtk_accelerator_parse(accelerator, &kb.key, &kb.mods);
    return kb;
}

static int keybind_match(Keybind kb, GdkEventKey key)
{
    return key.keyval == kb.key && kb.mods == (key.state & gtk_accelerator_get_default_mod_mask());
}

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

static void ls_app_window_clear_game(LSAppWindow* win)
{
    GdkScreen* screen;
    GList* l;

    atomic_store(&run_finished, false);

    gtk_widget_hide(win->box);
    gtk_widget_show_all(win->welcome);

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->clear_game) {
            component->ops->clear_game(component);
        }
    }

    // remove game's style
    if (win->style) {
        screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_remove_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(win->style));
        g_object_unref(win->style);
        win->style = NULL;
    }
}

// Forward declarations
static void timer_start(LSAppWindow* win, bool updateComponents);
static void timer_stop(LSAppWindow* win);
static void timer_split(LSAppWindow* win, bool updateComponents);
static void timer_reset(LSAppWindow* win);

static gboolean ls_app_window_step(gpointer data)
{
    LSAppWindow* win = data;
    long long now = ls_time_now();
    static int set_cursor;
    if (win->hide_cursor && !set_cursor) {
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

static int ls_app_window_find_theme(const LSAppWindow* win,
    const char* theme_name,
    const char* theme_variant,
    char* str)
{
    char theme_path[PATH_MAX];
    struct stat st = { 0 };
    if (!theme_name || !strlen(theme_name)) {
        str[0] = '\0';
        return 0;
    }
    strcpy(theme_path, "/");
    strcat(theme_path, theme_name);
    strcat(theme_path, "/");
    strcat(theme_path, theme_name);
    if (theme_variant && strlen(theme_variant)) {
        strcat(theme_path, "-");
        strcat(theme_path, theme_variant);
    }
    strcat(theme_path, ".css");

    strcpy(str, win->data_path);
    strcat(str, "/themes");
    strcat(str, theme_path);
    if (stat(str, &st) == -1) {
        return 0;
    }
    return 1;
}

static void ls_app_window_show_game(LSAppWindow* win)
{
    GdkScreen* screen;
    char str[PATH_MAX];
    GList* l;

    // set dimensions
    if (win->game->width > 0 && win->game->height > 0) {
        gtk_widget_set_size_request(GTK_WIDGET(win),
            win->game->width,
            win->game->height);
    }

    // set game theme
    if (ls_app_window_find_theme(win,
            win->game->theme,
            win->game->theme_variant,
            str)) {
        win->style = gtk_css_provider_new();
        screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(
            GTK_CSS_PROVIDER(win->style),
            str, NULL);
    }

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->show_game) {
            component->ops->show_game(component, win->game, win->timer);
        }
    }

    gtk_widget_show(win->box);
    gtk_widget_hide(win->welcome);
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

static void timer_start_split(LSAppWindow* win)
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

static void timer_stop_reset(LSAppWindow* win)
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

static void timer_cancel_run(LSAppWindow* win)
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

static void timer_skip(LSAppWindow* win)
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

static void timer_unsplit(LSAppWindow* win)
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

static void toggle_decorations(LSAppWindow* win)
{
    gtk_window_set_decorated(GTK_WINDOW(win), !win->decorated);
    win->decorated = !win->decorated;
}

static void toggle_win_on_top(LSAppWindow* win)
{
    gtk_window_set_keep_above(GTK_WINDOW(win), !win->win_on_top);
    win->win_on_top = !win->win_on_top;
}

static void keybind_start_split(GtkWidget* widget, LSAppWindow* win)
{
    timer_start_split(win);
}

static void keybind_stop_reset(const char* str, LSAppWindow* win)
{
    timer_stop_reset(win);
}

static void keybind_cancel(const char* str, LSAppWindow* win)
{
    timer_cancel_run(win);
}

static void keybind_skip(const char* str, LSAppWindow* win)
{
    timer_skip(win);
}

static void keybind_unsplit(const char* str, LSAppWindow* win)
{
    timer_unsplit(win);
}

static void keybind_toggle_decorations(const char* str, LSAppWindow* win)
{
    toggle_decorations(win);
}

static void keybind_toggle_win_on_top(const char* str, LSAppWindow* win)
{
    toggle_win_on_top(win);
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

static gboolean ls_app_window_keypress(GtkWidget* widget,
    GdkEvent* event,
    gpointer data)
{
    LSAppWindow* win = (LSAppWindow*)data;
    if (keybind_match(win->keybind_start_split, event->key)) {
        timer_start_split(win);
    } else if (keybind_match(win->keybind_stop_reset, event->key)) {
        timer_stop_reset(win);
    } else if (keybind_match(win->keybind_cancel, event->key)) {
        timer_cancel_run(win);
    } else if (keybind_match(win->keybind_unsplit, event->key)) {
        timer_unsplit(win);
    } else if (keybind_match(win->keybind_skip_split, event->key)) {
        timer_skip(win);
    } else if (keybind_match(win->keybind_toggle_decorations, event->key)) {
        toggle_decorations(win);
    } else if (keybind_match(win->keybind_toggle_win_on_top, event->key)) {
        toggle_win_on_top(win);
    }
    return TRUE;
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
    GtkCssProvider* provider;
    GdkScreen* screen;
    char str[PATH_MAX];
    const char* theme;
    const char* theme_variant;
    int i;

    win->display = gdk_display_get_default();
    win->style = NULL;

    // make data path
    win->data_path[0] = '\0';
    get_libresplit_folder_path(win->data_path);

    // load settings
    win->hide_cursor = json_boolean_value(get_setting_value("libresplit", "hide_cursor"));
    win->global_hotkeys = json_boolean_value(get_setting_value("libresplit", "global_hotkeys"));
    win->keybind_start_split = parse_keybind(
        json_string_value(get_setting_value("keybinds", "start_split")));
    win->keybind_stop_reset = parse_keybind(
        json_string_value(get_setting_value("keybinds", "stop_reset")));
    win->keybind_cancel = parse_keybind(
        json_string_value(get_setting_value("keybinds", "cancel")));
    win->keybind_unsplit = parse_keybind(
        json_string_value(get_setting_value("keybinds", "unsplit")));
    win->keybind_skip_split = parse_keybind(
        json_string_value(get_setting_value("keybinds", "skip_split")));
    win->keybind_toggle_decorations = parse_keybind(
        json_string_value(get_setting_value("keybinds", "toggle_decorations")));
    win->decorated = json_boolean_value(get_setting_value("libresplit", "start_decorated"));
    gtk_window_set_decorated(GTK_WINDOW(win), win->decorated);
    win->keybind_toggle_win_on_top = parse_keybind(
        json_string_value(get_setting_value("keybinds", "toggle_win_on_top")));
    win->win_on_top = json_boolean_value(get_setting_value("libresplit", "start_on_top"));
    gtk_window_set_keep_above(GTK_WINDOW(win), win->win_on_top);

    // Load CSS defaults
    provider = gtk_css_provider_new();
    screen = gdk_display_get_default_screen(win->display);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_css_provider_load_from_data(
        GTK_CSS_PROVIDER(provider),
        (char*)css_data, css_data_len, NULL);
    g_object_unref(provider);

    // Load theme
    theme = json_string_value(get_setting_value("libresplit", "theme"));
    theme_variant = json_string_value(get_setting_value("libresplit", "theme_variant"));
    if (ls_app_window_find_theme(win, theme, theme_variant, str)) {
        provider = gtk_css_provider_new();
        screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(
            GTK_CSS_PROVIDER(provider),
            str, NULL);
        g_object_unref(provider);
    }

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

    const bool enable_global_hotkeys = win->global_hotkeys && (force_global_hotkeys || !is_wayland);

    if (enable_global_hotkeys) {
        keybinder_init();
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "start_split")),
            (KeybinderHandler)keybind_start_split,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "stop_reset")),
            (KeybinderHandler)keybind_stop_reset,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "cancel")),
            (KeybinderHandler)keybind_cancel,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "unsplit")),
            (KeybinderHandler)keybind_unsplit,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "skip_split")),
            (KeybinderHandler)keybind_skip,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "toggle_decorations")),
            (KeybinderHandler)keybind_toggle_decorations,
            win);
        keybinder_bind(
            json_string_value(get_setting_value("keybinds", "toggle_win_on_top")),
            (KeybinderHandler)keybind_toggle_win_on_top,
            win);
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

    win->welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(win->welcome, 0);
    add_class(win->welcome, "welcome-screen");
    gtk_widget_set_margin_bottom(win->welcome, 0);
    gtk_widget_set_vexpand(win->welcome, TRUE);
    gtk_container_add(GTK_CONTAINER(win->container), win->welcome);
    win->welcome_lbl = gtk_label_new("Welcome to LibreSplit!\nNo split is currently loaded.\nRight click this window to open a split JSON file!");
    gtk_container_add(GTK_CONTAINER(win->welcome), win->welcome_lbl);

    win->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(win->welcome, "main-screen");
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

    g_timeout_add(1, ls_app_window_step, win);
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

struct _LSApp {
    GtkApplication parent;
};

struct _LSAppClass {
    GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(LSApp, ls_app, GTK_TYPE_APPLICATION)

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
    const char* last_split_folder = json_string_value(get_setting_value("history", "last_split_folder"));
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
        ls_update_setting("history", "last_split_folder", json_string(last_folder));
        ls_app_window_open(win, filename);
        ls_update_setting("history", "split_file", json_string(filename));
        g_free(filename);
    } else {
        gtk_widget_show_all(win->welcome);
    }
    gtk_widget_destroy(dialog);
}

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
    const char* last_auto_splitter_folder = json_string_value(get_setting_value("history", "last_auto_splitter_folder"));
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
        ls_update_setting("history", "last_auto_splitter_folder", json_string(last_folder));
        strcpy(auto_splitter_file, filename);
        ls_update_setting("history", "auto_splitter_file", json_string(filename));

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
        ls_game_update_splits(win->game, win->timer);
        save_game(win->game);
    }
}

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
    gtk_widget_set_size_request(GTK_WIDGET(win), -1, -1);
}

static void quit_activated(GSimpleAction* action,
    GVariant* parameter,
    gpointer app)
{
    exit(0);
}

static void toggle_auto_splitter(GtkCheckMenuItem* menu_item, gpointer user_data)
{
    gboolean active = gtk_check_menu_item_get_active(menu_item);
    if (active) {
        atomic_store(&auto_splitter_enabled, 1);
        ls_update_setting("libresplit", "auto_splitter_enabled", json_true());
    } else {
        atomic_store(&auto_splitter_enabled, 0);
        ls_update_setting("libresplit", "auto_splitter_enabled", json_false());
    }
}

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
    gtk_window_set_keep_above(GTK_WINDOW(win), !win->win_on_top);
    win->win_on_top = active;
}

// Create the context menu
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
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_enable_win_on_top), win->win_on_top);
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

static void ls_app_activate(GApplication* app)
{
    LSAppWindow* win;
    win = ls_app_window_new(LS_APP(app));
    gtk_window_present(GTK_WINDOW(win));
    if (get_setting_value("history", "split_file") != NULL) {
        // Check if split file exists
        struct stat st = { 0 };
        char splits_path[PATH_MAX];
        strcpy(splits_path, json_string_value(get_setting_value("history", "split_file")));
        if (stat(splits_path, &st) == -1) {
            printf("Split JSON %s does not exist\n", splits_path);
            open_activated(NULL, NULL, app);
        } else {
            ls_app_window_open(win, splits_path);
        }
    } else {
        open_activated(NULL, NULL, app);
    }
    if (get_setting_value("history", "auto_splitter_file") != NULL) {
        struct stat st = { 0 };
        char auto_splitters_path[PATH_MAX];
        strcpy(auto_splitters_path, json_string_value(get_setting_value("history", "auto_splitter_file")));
        if (stat(auto_splitters_path, &st) == -1) {
            printf("Auto Splitter %s does not exist\n", auto_splitters_path);
        } else {
            strcpy(auto_splitter_file, auto_splitters_path);
        }
    }
    if (get_setting_value("libresplit", "auto_splitter_enabled") != NULL) {
        if (json_is_false(get_setting_value("libresplit", "auto_splitter_enabled"))) {
            atomic_store(&auto_splitter_enabled, 0);
        } else {
            atomic_store(&auto_splitter_enabled, 1);
        }
    }
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

static void* ls_auto_splitter(void* arg)
{
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

static void dialog_response_cb(GtkWidget* dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_OK) {
        gtk_show_uri_on_window(GTK_WINDOW(NULL), "https://github.com/LibreSplit/LibreSplit/wiki/troubleshooting", 0, NULL);
    }
    gtk_widget_destroy(dialog);
}

gboolean display_non_capable_mem_read_dialog(gpointer data)
{
    atomic_store(&auto_splitter_enabled, 0);
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(NULL),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_NONE,
        "LibreSplit was unable to read memory from the target process.\n"
        "This is most probably due to insufficient permissions.\n"
        "This only happens on linux native games/binaries.\n"
        "Try running the game/program via steam.\n"
        "Autosplitter has been disabled.\n"
        "This warning will only show once until libresplit restarts.\n"
        "Please read the troubleshooting documentation to solve this error without running as root if the above doesnt work\n"
        "");

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
        "Close", GTK_RESPONSE_CANCEL,
        "Open documentation", GTK_RESPONSE_OK, NULL);

    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response_cb), NULL);
    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    // Connect the response signal to the callback function
    return FALSE; // False removes this function from the queue
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
