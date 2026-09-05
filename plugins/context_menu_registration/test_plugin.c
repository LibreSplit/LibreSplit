#include "glib-object.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include <gtk/gtk.h>

const abi_version_t abi_version = 1; // v0.1
const char plugin_name[] = "Test Plugin 4";
const char plugin_description[] = "Does something: just registers a fake plugin context menu item";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

void show_plugin_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    GtkWidget* dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "This is a dialog from a plugin");
    gtk_window_set_title(GTK_WINDOW(dialog), "Just a dialog");
    gtk_dialog_run(GTK_DIALOG(dialog));
}

int register_context_menu(GtkWidget* parent)
{
    GtkWidget* menu = gtk_menu_item_new_with_label("Plugin 4");
    gtk_menu_shell_append(GTK_MENU_SHELL(parent), menu);
    g_signal_connect(menu, "activate", G_CALLBACK(show_plugin_dialog), NULL);
    return 0;
}

int plug_init(PlugAPI* api)
{
    return 0;
}
