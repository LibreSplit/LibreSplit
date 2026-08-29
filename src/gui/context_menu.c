#include "src/gui/actions.h"
#include "src/gui/app_window.h"
#include "src/gui/help_dialog.h"
#include "src/gui/settings_dialog.h"
#include "src/lasr/auto-splitter.h"
#include <gtk/gtk.h>

// standardized cross-platform cursor names
static const char* const resize_cursors[] = {
    [GDK_SURFACE_EDGE_NORTH_WEST] = "nwse-resize",
    [GDK_SURFACE_EDGE_NORTH] = "ns-resize",
    [GDK_SURFACE_EDGE_NORTH_EAST] = "nesw-resize",
    [GDK_SURFACE_EDGE_WEST] = "ew-resize",
    [GDK_SURFACE_EDGE_EAST] = "ew-resize",
    [GDK_SURFACE_EDGE_SOUTH_WEST] = "nesw-resize",
    [GDK_SURFACE_EDGE_SOUTH] = "ns-resize",
    [GDK_SURFACE_EDGE_SOUTH_EAST] = "nwse-resize",
};

static const GActionEntry context_menu_actions[] = {
    { "open-splits", open_activated },
    { "save-splits", save_activated },
    { "open-auto-splitter", open_auto_splitter },
    { "enable-auto-splitter", NULL, NULL, "false", toggle_auto_splitter },
    { "reload", reload_activated },
    { "close", close_activated },
    { "always-on-top", NULL, NULL, "false", menu_toggle_win_on_top },
    { "settings", show_settings_dialog },
    { "about-and-help", show_help_dialog },
    { "quit", quit_activated },
};

/**
 * Syncs the context-menu toggles with application state
 *
 * @param win The application window that owns the actions
 */
static void sync_context_menu_state(LSAppWindow* win)
{
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "enable-auto-splitter");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(atomic_load(&auto_splitter_enabled)));

    action = g_action_map_lookup_action(G_ACTION_MAP(win), "always-on-top");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(win->opts.win_on_top));
}

/**
 * Determines which window edge the pointer is hovering over
 *
 * @param widget The widget referenced by the coordinates
 * @param x The pointer's x-coordinate
 * @param y The pointer's y-coordinate
 * @param edge A reference to the GdkSurfaceEdge to save the value to
 * @return bool Whether or not an edge was detected
 */
static bool get_window_edge(GtkWidget* widget, double x, double y, GdkSurfaceEdge* edge)
{
    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    bool left = x < WINDOW_PAD;
    bool right = x >= width - WINDOW_PAD;
    bool top = y < WINDOW_PAD;
    bool bot = y >= height - WINDOW_PAD;

    if (top && left) {
        *edge = GDK_SURFACE_EDGE_NORTH_WEST;
    } else if (top && right) {
        *edge = GDK_SURFACE_EDGE_NORTH_EAST;
    } else if (bot && left) {
        *edge = GDK_SURFACE_EDGE_SOUTH_WEST;
    } else if (bot && right) {
        *edge = GDK_SURFACE_EDGE_SOUTH_EAST;
    } else if (top) {
        *edge = GDK_SURFACE_EDGE_NORTH;
    } else if (bot) {
        *edge = GDK_SURFACE_EDGE_SOUTH;
    } else if (left) {
        *edge = GDK_SURFACE_EDGE_WEST;
    } else if (right) {
        *edge = GDK_SURFACE_EDGE_EAST;
    } else {
        return false;
    }

    return true;
}

/**
 * Begins an interactive move, or an edge resize for an undecorated window.
 *
 * @param gesture The click gesture receiving the primary-button press
 * @param x The press x-coordinate
 * @param y The press y-coordinate
 */
void button_left_click(GtkGestureClick* gesture, double x, double y)
{
    GtkEventController* controller = GTK_EVENT_CONTROLLER(gesture);
    LSAppWindow* win = LS_APP_WINDOW(gtk_event_controller_get_widget(controller));
    GdkSurfaceEdge edge;
    bool resize = !gtk_window_get_decorated(GTK_WINDOW(win))
        && get_window_edge(gtk_event_controller_get_widget(controller), x, y, &edge);

    GdkEvent* event = gtk_event_controller_get_current_event(controller);
    GdkDevice* device = gtk_event_controller_get_current_event_device(controller);
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(win));
    double surface_x;
    double surface_y;

    if (event == NULL
        || device == NULL
        || surface == NULL
        || !GDK_IS_TOPLEVEL(surface)
        || !gdk_event_get_position(event, &surface_x, &surface_y)) {
        return;
    }

    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    if (resize) {
        gdk_toplevel_begin_resize(GDK_TOPLEVEL(surface),
            edge,
            device,
            GDK_BUTTON_PRIMARY,
            surface_x,
            surface_y,
            gtk_event_controller_get_current_event_time(controller));
    } else {
        gdk_toplevel_begin_move(GDK_TOPLEVEL(surface),
            device,
            GDK_BUTTON_PRIMARY,
            surface_x,
            surface_y,
            gtk_event_controller_get_current_event_time(controller));
    }
    gtk_event_controller_reset(controller);
}

/**
 * Creates the context menu and its window-scoped actions.
 *
 * @param win The application window that owns the menu
 * @param app The LibreSplit application passed to menu action callbacks
 */
static void create_context_menu(LSAppWindow* win, gpointer app)
{
    GMenu* menu = g_menu_new();
    GMenu* section = g_menu_new();

    g_action_map_add_action_entries(G_ACTION_MAP(win),
        context_menu_actions,
        G_N_ELEMENTS(context_menu_actions),
        app);

    g_menu_append(section, "Open Splits", "win.open-splits");
    g_menu_append(section, "Save Splits", "win.save-splits");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Open Auto Splitter", "win.open-auto-splitter");
    g_menu_append(section, "Enable Auto Splitter", "win.enable-auto-splitter");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Reload", "win.reload");
    g_menu_append(section, "Close", "win.close");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Always on Top", "win.always-on-top");
    g_menu_append(section, "Settings", "win.settings");
    g_menu_append(section, "About and help", "win.about-and-help");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Quit", "win.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    win->context_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    gtk_popover_set_has_arrow(GTK_POPOVER(win->context_menu), FALSE);
    gtk_widget_set_parent(win->context_menu, GTK_WIDGET(win));
    g_object_unref(menu);
}

/**
 * Opens the context menu at the pointer position.
 *
 * @param gesture The click gesture receiving the secondary-button press
 * @param x The press x-coordinate in the application window allocation
 * @param y The press y-coordinate in the application window allocation
 * @param app Pointer to the LibreSplit application
 */
void button_right_click(GtkGestureClick* gesture, double x, double y, gpointer app)
{
    LSAppWindow* win = LS_APP_WINDOW(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));

    if (win->context_menu == NULL) {
        create_context_menu(win, app);
    }

    sync_context_menu_state(win);

    GdkRectangle pointing_to = {
        .x = (int)x,
        .y = (int)y,
        .width = 1,
        .height = 1,
    };

    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    gtk_popover_set_pointing_to(GTK_POPOVER(win->context_menu), &pointing_to);
    gtk_popover_popup(GTK_POPOVER(win->context_menu));
}

/**
 * Delegates supported pointer presses to their respective handlers.
 *
 * Primary-button presses move the window or resize it from an undecorated
 * edge. Secondary-button presses open the LibreSplit context menu.
 *
 * @param gesture The click gesture receiving the button press
 * @param n_press The number of presses in the current sequence
 * @param x The press x-coordinate in the application window allocation
 * @param y The press y-coordinate in the application window allocation
 * @param app Pointer to the LibreSplit application
 */
void handle_button_pressed(GtkGestureClick* gesture, int n_press, double x, double y, gpointer app)
{
    switch (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture))) {
        case GDK_BUTTON_PRIMARY:
            button_left_click(gesture, x, y);
            break;
        case GDK_BUTTON_SECONDARY:
            button_right_click(gesture, x, y, app);
            break;
    }
}

/**
 * Displays the appropriate resize cursor while the pointer is over an edge
 * of an undecorated window.
 *
 * @param controller The motion controller attached to the application window
 * @param x The pointer x-coordinate in the application window allocation
 * @param y The pointer y-coordinate in the application window allocation
 * @param data Pointer to the LibreSplit application window
 */
void handle_pointer_motion(GtkEventControllerMotion* controller, double x, double y, gpointer data)
{
    LSAppWindow* win = LS_APP_WINDOW(data);
    GtkWidget* widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(controller));

    if (win->opts.hide_cursor) {
        if (win->resize_cursor_hover) {
            gtk_widget_set_cursor_from_name(widget, "none");
            win->resize_cursor_hover = false;
        }
        return;
    }

    // if decorations are enabled, decorations handle resize
    if (gtk_window_get_decorated(GTK_WINDOW(win))) {
        if (win->resize_cursor_hover) {
            gtk_widget_set_cursor_from_name(widget, NULL);
            win->resize_cursor_hover = false;
        }
        return;
    }

    GdkSurfaceEdge edge;
    const char* cursor = NULL;

    if (get_window_edge(widget, x, y, &edge)) {
        if (win->resize_cursor_hover && win->resize_cursor_edge == edge) {
            return;
        }

        cursor = resize_cursors[edge];
    } else if (!win->resize_cursor_hover) {
        return;
    }

    gtk_widget_set_cursor_from_name(widget, cursor);
    win->resize_cursor_hover = cursor != NULL;
    if (win->resize_cursor_hover) {
        win->resize_cursor_edge = edge;
    }
}

/**
 * Clears edge-resize cursor state when the pointer leaves the window.
 *
 * @param controller The motion controller attached to the application window
 * @param data Pointer to the LibreSplit application window
 */
void handle_pointer_leave(GtkEventControllerMotion* controller, gpointer data)
{
    LSAppWindow* win = LS_APP_WINDOW(data);

    if (win->opts.hide_cursor) {
        win->resize_cursor_hover = false;
        return;
    }

    if (win->resize_cursor_hover) {
        gtk_widget_set_cursor_from_name(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller)), NULL);
        win->resize_cursor_hover = false;
    }
}
