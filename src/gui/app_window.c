#include "app_window.h"
#include "src/gui/actions.h"
#include "src/gui/backends/x11.h"
#include "src/gui/component/components.h"
#include "src/gui/context_menu.h"
#include "src/gui/dialogs.h"
#include "src/gui/game.h"
#include "src/gui/theming.h"
#include "src/gui/timer.h"
#include "src/gui/widgets/alert.h"
#include "src/keybinds/delayed_callbacks.h"
#include "src/keybinds/keybinds_callbacks.h"
#include "src/lasr/auto-splitter.h"
#include "src/logging.h"
#include "src/settings/settings.h"
#include "src/settings/utils.h"
#include "src/timer.h"

#include <glib-object.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>

extern atomic_bool exit_requested; /*!< Set to 1 when LibreSplit is exiting */

static void ls_app_window_size_allocate(GtkWidget* widget, int width, int height, int baseline);

static void ls_app_init(LSApp* app)
{
}

G_DEFINE_TYPE(LSApp, ls_app, GTK_TYPE_APPLICATION)

G_DEFINE_TYPE(LSAppWindow, ls_app_window, GTK_TYPE_APPLICATION_WINDOW)

/**
 * Sets whether or not the window should be decorated
 * based on the user's preferences.
 *
 * @param win The current main app window
 */
void set_window_decorations(LSAppWindow* win)
{
    gtk_window_set_decorated(GTK_WINDOW(win), win->opts.decorated);
}

void toggle_decorations(LSAppWindow* win)
{
    LOG_DEBUG("Toggling window decorations");
    win->opts.decorated = !win->opts.decorated;
    set_window_decorations(win);
    cfg.libresplit.start_decorated.value.b = win->opts.decorated;
    config_save();
}

void toggle_win_on_top(LSAppWindow* win)
{
    gboolean active = !win->opts.win_on_top;
    LOG_DEBUG("Toggling 'Always on Top' window flag");
    x11_set_keep_above(GTK_WINDOW(win), active);
    win->opts.win_on_top = active;
    cfg.libresplit.start_on_top.value.b = win->opts.win_on_top;
    config_save();

    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "always-on-top");
    if (action != NULL) {
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(active));
    }
}

/**
 * Applies always-on-top state once the window is mapped.
 *
 * @param widget The mapped LibreSplit window
 * @param data Pointer to the LibreSplit window state
 */
static void ls_app_window_map(GtkWidget* widget, gpointer data)
{
    LSAppWindow* win = LS_APP_WINDOW(data);
    x11_set_keep_above(GTK_WINDOW(widget), win->opts.win_on_top);
}

LSAppWindow* ls_app_window_new(LSApp* app)
{
    LOG_DEBUG("Creating a new LibreSplit window");
    LSAppWindow* win;
    win = g_object_new(LS_APP_WINDOW_TYPE, "application", app, NULL);
    return win;
}

void ls_app_window_open(LSAppWindow* win, const char* file)
{
    LOG_DEBUG("Opening LibreSplit window");
    char* error_msg = NULL;

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
            // max file size + reasonable error message length
            char msg[384];
            snprintf(msg, sizeof msg, "%s\n%s", "some-error", file);
            ls_alert_error(GTK_WINDOW(win), "LibreSplit", "JSON parse error:", msg);
            free(error_msg);
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
    LOG_DEBUG("Initializing configuration");
    if (!config_init()) {
        LOG_WARN("Configuration failed to load, will use defaults");
    }

    LSAppWindow* win;
    win = ls_app_window_new(LS_APP(app));
    gtk_window_present(GTK_WINDOW(win));

    if (cfg.history.split_file.value.s[0] != '\0') {
        LOG_DEBUG("Loading last used split file from history");
        // Check if split file exists
        struct stat st = { 0 };
        char splits_path[PATH_MAX];
        strcpy(splits_path, cfg.history.split_file.value.s);
        if (stat(splits_path, &st) == -1) {
            LOG_INFOF("Split JSON %s does not exist", splits_path);
            open_activated(NULL, NULL, app);
        } else {
            ls_app_window_open(win, splits_path);
        }
    } else {
        LOG_DEBUG("Opening split file selection dialog");
        open_activated(NULL, NULL, app);
    }
    if (cfg.history.auto_splitter_file.value.s[0] != '\0') {
        LOG_DEBUG("Opening last used auto splitter from history");
        struct stat st = { 0 };
        char auto_splitters_path[PATH_MAX];
        strcpy(auto_splitters_path, cfg.history.auto_splitter_file.value.s);
        if (stat(auto_splitters_path, &st) == -1) {
            LOG_INFOF("Auto Splitter %s does not exist", auto_splitters_path);
        } else {
            strcpy(auto_splitter_file, auto_splitters_path);
        }
    }
    atomic_store(&auto_splitter_enabled, cfg.libresplit.auto_splitter_enabled.value.b);

    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click),
        GTK_PHASE_CAPTURE);
    g_signal_connect(click, "pressed", G_CALLBACK(handle_button_pressed), app);
    gtk_widget_add_controller(GTK_WIDGET(win), GTK_EVENT_CONTROLLER(click));
}

void ls_app_open(GApplication* app,
    GFile** files,
    gint n_files,
    const gchar* hint)
{
    LOG_DEBUG("Starting LibreSplit App");
    int i;
    LSAppWindow* win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (!win) {
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
    GTK_WIDGET_CLASS(class)->size_allocate = ls_app_window_size_allocate;
}

static void ls_app_window_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    // This is the GtkApplicationWindow size allocate, not our appwindow
    GTK_WIDGET_CLASS(ls_app_window_parent_class)->size_allocate(widget, width, height, baseline);
    LSAppWindow* win = LS_APP_WINDOW(widget);
    if (win->context_menu != NULL) {
        gtk_popover_present(GTK_POPOVER(win->context_menu));
    }
}

/**
 * @brief The user was presented a confirm reset dialog while closing the app and chose yes.
 * This function performs the close operation after user confirmation.
 *
 * @param window The main application window
 */
static void user_allowed_close(gpointer window)
{
    gtk_window_destroy(GTK_WINDOW(window));
}

/**
 * Triggered when LibreSplit receives a notification to close.
 *
 * @param window The LibreSplit window being closed.
 * @param data Usually NULL.
 */
gboolean ls_app_window_delete(GtkWindow* window, gpointer data)
{
    LSAppWindow* win = LS_APP_WINDOW(window);

    // Warn if the reset will lose a gold split, and allow the user to cancel the reset if they want to keep it
    if (win->timer && win->timer->running && (ls_timer_has_gold_split(win->timer) || ls_timer_has_rainbow_split(win->timer))) {
        if (cfg.libresplit.ask_on_gold.value.b) {
            display_confirm_reset_dialog(user_allowed_close, window, NULL);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Closes LibreSplit.
 *
 * @param widget The pointer to the LibreSplit window, as a widget.
 * @param data Usually NULL.
 */
void ls_app_window_destroy(GtkWidget* widget, gpointer data)
{
    LOG_INFO("Exiting LibreSplit. GG!");
    LSAppWindow* win = (LSAppWindow*)widget;
    if (win->timer) {
        ls_timer_release(win->timer);
        win->timer = 0;
    }
    if (win->game) {
        ls_game_release(win->game);
        win->game = 0;
    }
    atomic_store(&auto_splitter_enabled, 0);
    atomic_store(&exit_requested, 1);
    LOG_DEBUG("Exit request sent to threads");
    if (win->context_menu) {
        gtk_widget_unparent(win->context_menu);
        win->context_menu = NULL;
    }
    // Close any other open application windows (settings, dialogs, etc.)
    GApplication* app = g_application_get_default();
    if (app) {
        GList* windows = gtk_application_get_windows(GTK_APPLICATION(app));
        GList* snapshot = g_list_copy(windows); // Copy to avoid race conditions
        for (GList* l = snapshot; l != NULL; l = l->next) {
            GtkWidget* w = GTK_WIDGET(l->data);
            if (w != GTK_WIDGET(win)) {
                gtk_window_destroy(GTK_WINDOW(w));
            }
        }
        g_list_free(snapshot);
    }
    close_logger();
    g_application_quit(G_APPLICATION(app));
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
        gtk_widget_set_cursor_from_name(GTK_WIDGET(win), "none");
        set_cursor = 1;
    }

    if (win->timer) {
        ls_timer_step(win->timer);

        // printf("RTA: %llu; LT: %llu; LRT: %llu; GT: %llu; GT?: %d\n",
        //     win->timer->realTime,
        //     win->timer->loadingTime,
        //     (win->timer->realTime - win->timer->loadingTime),
        //     win->timer->gameTime,
        //     win->timer->usingGameTime);

        if (atomic_load(&auto_splitter_enabled)) {
            if (atomic_load(&run_using_game_time_call)) {
                win->timer->usingGameTime = atomic_load(&run_using_game_time);
                atomic_store(&run_using_game_time_call, false);
            }
            if (atomic_load(&call_start)) {
                timer_start(win);
                atomic_store(&call_start, 0);
            }
            if (atomic_load(&call_split)) {
                timer_split(win);
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
            if (atomic_load(&update_game_time)) {
                // Update the timer with the game time from auto-splitter
                win->timer->gameTime = atomic_load(&game_time_value);
                atomic_store(&update_game_time, false);
            }
            if (atomic_load(&call_reset)) {
                timer_stop_and_reset(win);
                atomic_store(&run_using_game_time_call, true);
                atomic_store(&call_reset, 0);
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
        gtk_widget_queue_draw(GTK_WIDGET(win));
    }
    return TRUE;
}

/**
 * @brief A helper function to get the main LibreSplit AppWindow.
 * gtk_application_get_windows returns a list of windows ordered by the most
 * recently focused window. Therefore the first result is not gauranteed to be
 * the main window.
 *
 * @param app The application
 * @return LSAppWindow* The main application window or NULL if none exists
 */
LSAppWindow* ls_get_main_app_window(GtkApplication* app)
{
    for (GList* node = gtk_application_get_windows(app); node != NULL; node = node->next) {
        if (LS_IS_APP_WINDOW(node->data)) {
            return LS_APP_WINDOW(node->data);
        }
    }

    return NULL;
}

static void ls_app_window_init(LSAppWindow* win)
{
    LOG_DEBUG("Initializing LibreSplit Window");
    const char* theme;
    const char* theme_variant;
    int i;

    win->display = gdk_display_get_default();
    win->style = NULL;
    win->context_menu = NULL;
    win->resize_cursor_hover = false;

    // make data path
    win->data_path[0] = '\0';
    get_libresplit_folder_path(win->data_path);

    // load settings
    LOG_DEBUG("Loading Settings...");
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
    set_window_decorations(win);

    // Load theme
    LOG_DEBUG("Loading Theme...");
    theme = cfg.libresplit.theme.value.s;
    theme_variant = cfg.libresplit.theme_variant.value.s;
    ls_app_load_theme_with_fallback(win, theme, theme_variant);

    // Load window junk
    add_class(GTK_WIDGET(win), "window");
    add_class(GTK_WIDGET(win), "main-window");
    win->game = 0;
    win->timer = 0;

    LOG_DEBUG("Connecting window signals...")
    g_signal_connect(win, "close-request",
        G_CALLBACK(ls_app_window_delete), NULL);
    g_signal_connect(win, "destroy",
        G_CALLBACK(ls_app_window_destroy), NULL);
    g_signal_connect(win, "map",
        G_CALLBACK(ls_app_window_map), win);

    // As a crash workaround, only enable global hotkeys if not on Wayland
    const bool force_global_hotkeys = getenv("LIBRESPLIT_FORCE_GLOBAL_HOTKEYS");
    if (win->opts.global_hotkeys && (is_x11_display() || force_global_hotkeys)) {
        LOG_DEBUG("Global Hotkeys Enabled, binding hotkeys globally...");
        bind_global_hotkeys(cfg, win);
    } else {
        LOG_DEBUG("Global Hotkeys Disabled, binding hotkeys only to the main window...");
        GtkEventController* key_controller = gtk_event_controller_key_new();
        g_signal_connect(key_controller, "key-pressed", G_CALLBACK(ls_app_window_keypress), win);
        gtk_widget_add_controller(GTK_WIDGET(win), key_controller);
    }

    LOG_DEBUG("Creating the main window...");
    win->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(win->container, "libresplit-content");
    gtk_widget_set_margin_top(win->container, WINDOW_PAD);
    gtk_widget_set_margin_bottom(win->container, WINDOW_PAD);
    gtk_widget_set_vexpand(win->container, TRUE);
    gtk_window_set_child(GTK_WINDOW(win), win->container);

    GtkEventController* motion_controller = gtk_event_controller_motion_new();
    gtk_event_controller_set_propagation_phase(motion_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(motion_controller, "motion",
        G_CALLBACK(handle_pointer_motion), win);
    g_signal_connect(motion_controller, "leave",
        G_CALLBACK(handle_pointer_leave), win);
    gtk_widget_add_controller(GTK_WIDGET(win), motion_controller);

    LOG_DEBUG("Creating the welcome box...");
    win->welcome_box = welcome_box_new(win->container);

    win->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(win->welcome_box->box, "main-screen");
    gtk_widget_set_margin_top(win->box, 0);
    gtk_widget_set_margin_bottom(win->box, 0);
    gtk_widget_set_vexpand(win->box, TRUE);
    gtk_box_append(GTK_BOX(win->container), win->box);

    // Create all available components (TODO: change this in the future)
    LOG_DEBUG("Creating components...");
    win->components = NULL;
    for (i = 0; ls_components[i].name != NULL; i++) {
        LSComponent* component = ls_components[i].new();
        if (component) {
            GtkWidget* widget = component->ops->widget(component);
            if (widget) {
                gtk_widget_set_margin_start(widget, WINDOW_PAD);
                gtk_widget_set_margin_end(widget, WINDOW_PAD);
                gtk_box_append(GTK_BOX(win->box),
                    component->ops->widget(component));
            }
            win->components = g_list_append(win->components, component);
        }
    }

    // NOTE: This always creates an empty footer, no matter how many
    //  ^ "footers" are available, which may give issues with theming
    LOG_DEBUG("Creating window footer...");
    win->footer = gtk_grid_new();
    add_class(win->footer, "footer");
    gtk_widget_set_margin_start(win->footer, WINDOW_PAD);
    gtk_widget_set_margin_end(win->footer, WINDOW_PAD);
    gtk_box_append(GTK_BOX(win->box), win->footer);

    LOG_DEBUG("Setting up timers for updating and drawing the window...");
    // Update the internal state every millisecond
    g_timeout_add(1, ls_app_window_step, win);
    // Draw the window at 30 FPS
    g_timeout_add((int)(1000 / 30.), ls_app_window_draw, win);
}
