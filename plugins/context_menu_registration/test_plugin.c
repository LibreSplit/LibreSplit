#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include <gio/gio.h>
#include <gio/gmenu.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

/* =======================================
 * Metadata section
 * ---------------------------------------
 * Here we write all the metadata of the plugin, as well as
 * the supported ABI version
 */
const abi_version_t abi_version = 1; // v0.1
const char plugin_name[] = "Test Plugin 4";
const char plugin_description[] = "Does something: just registers a fake plugin context menu item";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

// Here we create a simple callback that will be called by a click on the menu item
// we're creating.
void print_a_string(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    g_print("This is a test from a plugin\n");
}

// This function is called automatically by LibreSplit if it is defined.
int register_context_menu(GMenu* parent, GtkWidget* window)
{
    // Create a action group (this way the actions are stored inside the plugin
    // and we won't mess with the internals of LibreSplit)
    GSimpleActionGroup* actions = g_simple_action_group_new();
    // Create an action and connect the callback we created earlier
    GSimpleAction* do_print = g_simple_action_new("print_stuff", NULL);
    g_signal_connect(do_print, "activate", G_CALLBACK(print_a_string), NULL);
    // Add the new action to the actions group
    g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(do_print));
    // We don't need the action anymore, it's managed by the group now.
    g_object_unref(do_print);
    // Insert the action group in the LibreSplit GTK window, naming the group
    // "plugin4"
    gtk_widget_insert_action_group(window, "plugin4", G_ACTION_GROUP(actions));
    // We don't need the action group anymore, it's managed by GTK now.
    g_object_unref(actions);
    // Now we append to the "Plugins" submenu an action called "Print Stuff"
    // that calls the "print_stuff" action under the group named "plugin4"
    g_menu_append(parent, "Print Stuff", "plugin4.print_stuff");
    // All done.
    return 0;
}

int plug_init(PlugAPI* api)
{
    // register_context_menu is called by LibreSplit automatically, we don't
    // need to do anything, but plug_init must be defined, even if a no-op
    return 0;
}
