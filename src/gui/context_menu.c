#include "src/gui/actions.h"
#include "src/gui/app_window.h"
#include "src/gui/help_dialog.h"
#include "src/gui/settings_dialog.h"
#include "src/lasr/auto-splitter.h"
#include <gtk/gtk.h>

/**
 * Creates the Context Menu.
 *
 * @param widget The widget that was right clicked. Not used here.
 * @param event The click event, containing which button was used to click.
 * @param app Pointer to the LibreSplit application.
 *
 * @return True if the click was done with the RMB (and a context menu was shown), False otherwise.
 */
gboolean button_right_click(GtkWidget* widget, GdkEventButton* event, gpointer app)
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
        if (win->context_menu == NULL) {
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
            GtkWidget* menu_settings = gtk_menu_item_new_with_label("Settings");
            GtkWidget* menu_about = gtk_menu_item_new_with_label("About and help");
            GtkWidget* menu_quit = gtk_menu_item_new_with_label("Quit");

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
        return TRUE;
    }
    return FALSE;
}
