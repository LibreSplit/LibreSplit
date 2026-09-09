#include "gui/actions.h"
#include "gui/app_window.h"
#include "gui/help_dialog.h"
#include "gui/settings_dialog.h"
#include "lasr/auto-splitter.h"
#include "plugins/plugin_loading.h"
#include <gtk/gtk.h>

// standardized cross-platform cursor names
static const char* const resize_cursors[] = {
    [GDK_WINDOW_EDGE_NORTH_WEST] = "nwse-resize",
    [GDK_WINDOW_EDGE_NORTH] = "ns-resize",
    [GDK_WINDOW_EDGE_NORTH_EAST] = "nesw-resize",
    [GDK_WINDOW_EDGE_WEST] = "ew-resize",
    [GDK_WINDOW_EDGE_EAST] = "ew-resize",
    [GDK_WINDOW_EDGE_SOUTH_WEST] = "nesw-resize",
    [GDK_WINDOW_EDGE_SOUTH] = "ns-resize",
    [GDK_WINDOW_EDGE_SOUTH_EAST] = "nwse-resize",
};

/**
 * Get the target widget's top most parent window coordinates from the base event coordinates.
 *
 * @param widget The current event's widget
 * @param window The current event's window
 * @param event_x The initial event x-coordinate
 * @param event_y The initial event y-coordinate
 * @param window_x Reference to the output's window x-coordinate
 * @param window_y Reference to the output's window y-coordinate
 * @return bool whether or not the coordinates were retrieved succesfully
 */
static bool get_window_coordinates(GtkWidget* widget, GdkWindow* window, double event_x, double event_y, double* window_x, double* window_y)
{
    *window_x = event_x;
    *window_y = event_y;
    GdkWindow* target = gtk_widget_get_window(widget);

    while (window && window != target) {
        double parent_x;
        double parent_y;

        gdk_window_coords_to_parent(window, *window_x, *window_y, &parent_x, &parent_y);

        *window_x = parent_x;
        *window_y = parent_y;
        window = gdk_window_get_effective_parent(window);
    }

    return window == target;
}

/**
 * Determines which (if any) window edge the event is nearest
 * and returns that edge to the GdkWindowEdge reference.
 *
 * @param widget The current event's widget
 * @param window The current event's window
 * @param x The event's x-coordinate
 * @param y The event's y-coordinate
 * @param edge A reference to the GdkWindowEdge to save the value to
 * @return bool Whether or not an edge was detected
 */
static bool get_window_edge(GtkWidget* widget, GdkWindow* window, double x, double y, GdkWindowEdge* edge)
{
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    double window_x;
    double window_y;

    if (!get_window_coordinates(widget, window, x, y, &window_x, &window_y)) {
        return false;
    }

    bool left = window_x < WINDOW_PAD;
    bool right = window_x >= width - WINDOW_PAD;
    bool top = window_y < WINDOW_PAD;
    bool bot = window_y >= height - WINDOW_PAD;

    if (top && left) {
        *edge = GDK_WINDOW_EDGE_NORTH_WEST;
    } else if (top && right) {
        *edge = GDK_WINDOW_EDGE_NORTH_EAST;
    } else if (bot && left) {
        *edge = GDK_WINDOW_EDGE_SOUTH_WEST;
    } else if (bot && right) {
        *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
    } else if (top) {
        *edge = GDK_WINDOW_EDGE_NORTH;
    } else if (bot) {
        *edge = GDK_WINDOW_EDGE_SOUTH;
    } else if (left) {
        *edge = GDK_WINDOW_EDGE_WEST;
    } else if (right) {
        *edge = GDK_WINDOW_EDGE_EAST;
    } else {
        return false;
    }

    return true;
}

/**
 * Handles left mouse button clicks. We first detect if the mouse is over the very edge of the window
 * if it is, then the click is determined to be for resizing the window and window resizing is handled.
 * Otherwise, the event is interpreted to be for moving the window itself.
 *
 * @param widget The widget that was left clicked.
 * @param event The click event, containing which button was used to click.
 */
void button_left_click(GtkWidget* widget, GdkEventButton* event)
{
    GdkWindowEdge edge;

    // If the window is decorated then we should only ever worry about handling in app moves
    if (gtk_window_get_decorated(GTK_WINDOW(widget)) || !get_window_edge(widget, event->window, event->x, event->y, &edge)) {
        gtk_window_begin_move_drag(GTK_WINDOW(widget), event->button, event->x_root, event->y_root, event->time);
        return;
    }

    gtk_window_begin_resize_drag(GTK_WINDOW(widget), edge, event->button, event->x_root, event->y_root, event->time);
}

/**
 * Creates the Context Menu.
 *
 * @param event The click event, containing which button was used to click.
 * @param app Pointer to the LibreSplit application.
 */
void button_right_click(GdkEventButton* event, gpointer app)
{
    GList* windows = gtk_application_get_windows(GTK_APPLICATION(app));
    LSAppWindow* win = windows ? LS_APP_WINDOW(windows->data) : ls_app_window_new(LS_APP(app));

    if (win->context_menu == NULL) {
        GtkWidget* menu = gtk_menu_new();
        GtkWidget* menu_open_splits = gtk_menu_item_new_with_label("Open Splits");
        GtkWidget* menu_save_splits = gtk_menu_item_new_with_label("Save Splits");
        GtkWidget* menu_open_auto_splitter = gtk_menu_item_new_with_label("Open Auto Splitter");
        GtkWidget* menu_enable_auto_splitter = gtk_check_menu_item_new_with_label("Enable Auto Splitter");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_enable_auto_splitter), atomic_load(&auto_splitter_enabled));
        GtkWidget* menu_enable_win_on_top = gtk_check_menu_item_new_with_label("Always on Top");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_enable_win_on_top), win->opts.win_on_top);
        GtkWidget* menu_plugins = gtk_menu_item_new_with_label("Plugins");
        GtkWidget* menu_reload = gtk_menu_item_new_with_label("Reload");
        GtkWidget* menu_close = gtk_menu_item_new_with_label("Close");
        GtkWidget* menu_settings = gtk_menu_item_new_with_label("Settings");
        GtkWidget* menu_about = gtk_menu_item_new_with_label("About and help");
        GtkWidget* menu_quit = gtk_menu_item_new_with_label("Quit");

        create_plugin_context_menus(menu_plugins);

        // Add the menu items to the menu
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_open_splits);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_save_splits);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_open_auto_splitter);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_enable_auto_splitter);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_reload);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_close);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_plugins);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_enable_win_on_top);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_settings);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_about);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_quit);

        // Attach the callback functions to the menu items
        g_signal_connect(menu_open_splits, "activate", G_CALLBACK(open_activated), app);
        g_signal_connect(menu_save_splits, "activate", G_CALLBACK(save_activated), app);
        g_signal_connect(menu_open_auto_splitter, "activate", G_CALLBACK(open_auto_splitter), app);
        g_signal_connect(menu_enable_auto_splitter, "toggled", G_CALLBACK(toggle_auto_splitter), NULL);
        g_signal_connect(menu_enable_win_on_top, "toggled", G_CALLBACK(menu_toggle_win_on_top), app);
        g_signal_connect(menu_reload, "activate", G_CALLBACK(reload_activated), app);
        g_signal_connect(menu_close, "activate", G_CALLBACK(close_activated), app);
        g_signal_connect(menu_settings, "activate", G_CALLBACK(show_settings_dialog), app);
        g_signal_connect(menu_about, "activate", G_CALLBACK(show_help_dialog), app);
        g_signal_connect(menu_quit, "activate", G_CALLBACK(quit_activated), app);

        win->context_menu = menu;
    }

    gtk_widget_show_all(win->context_menu);
    gtk_menu_popup_at_pointer(GTK_MENU(win->context_menu), (GdkEvent*)event);
}

/**
 * Event handler for a button being pressed on the main window.
 * This function delegates supported operations to their respective handler
 * depending on their buttons.
 *
 * GDK_BUTTON_PRIMARY - Left mouse button click, used for moving the window
 * GDK_BUTTON_SECONDARY - Right mouse button click, display right click context menu
 *
 * @param widget The widget that was right clicked.
 * @param event The click event, containing which button was used to click.
 * @param app Pointer to the LibreSplit application.
 * @return gboolean TRUE when a supported event is handled, FALSE otherwise.
 */
gboolean handle_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer app)
{
    switch (event->button) {
        case GDK_BUTTON_PRIMARY:
            button_left_click(widget, event);
            return TRUE;
        case GDK_BUTTON_SECONDARY:
            button_right_click(event, app);
            return TRUE;
    }

    return FALSE;
}

/**
 * Determines if the user's pointer is hovering over the application's edge
 * and adjusts the cursor accordingly to display resize cursors instead of the default
 * to indicate that the window is resizable and in which direction(s).
 *
 * This function does nothing when decorations are enabled as resizing is
 * left to the decorations to handle.
 *
 * Always returns FALSE as motion handling is not terminal.
 * This allows the signal to continue propagating.
 *
 * @param widget The widget that was hovered on
 * @param event The hover event, containing the pointer's coordinates
 * @param data not used
 * @return FALSE
 */
gboolean handle_pointer_motion(GtkWidget* widget, GdkEventMotion* event, gpointer data)
{
    LSAppWindow* win = LS_APP_WINDOW(widget);
    GdkWindow* window = gtk_widget_get_window(widget);

    if (win->opts.hide_cursor) {
        win->resize_cursor_hover = false;
        return FALSE;
    }

    // if decorations enabled, decorations handle resize
    if (gtk_window_get_decorated(GTK_WINDOW(widget))) {
        if (win->resize_cursor_hover) {
            gdk_window_set_cursor(window, NULL);
            win->resize_cursor_hover = false;
        }

        return FALSE;
    }

    GdkCursor* cursor = NULL;
    GdkWindowEdge edge;

    if (get_window_edge(widget, event->window, event->x, event->y, &edge)) {
        if (win->resize_cursor_hover && win->resize_cursor_edge == edge) {
            return FALSE;
        }

        cursor = gdk_cursor_new_from_name(gtk_widget_get_display(widget), resize_cursors[edge]);
    } else if (!win->resize_cursor_hover) {
        return FALSE;
    }

    gdk_window_set_cursor(window, cursor);
    win->resize_cursor_hover = cursor != NULL;
    if (win->resize_cursor_hover) {
        win->resize_cursor_edge = edge;
    }

    if (cursor) {
        g_object_unref(cursor);
    }

    return FALSE;
}
