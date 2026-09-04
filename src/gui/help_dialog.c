#include "help_dialog.h"
#include "app_window.h"
#include "src/gui/widgets/alert.h"
#include "src/logging.h"
#include <gtk/gtk.h>
#include <stdio.h>

static GtkWindow* help_window_singleton = NULL;

/**
 * Help window destructor.
 *
 * @param widget The GTK Help Window;
 * @param user_data Unused.
 */
static void on_help_window_destroy(GtkWidget* widget, gpointer user_data)
{
    LOG_DEBUG("Destroying Help Window...");
    help_window_singleton = NULL;
    dialog_count_dec();
}

/**
 * Closes the help window when Escape is pressed.
 *
 * @param controller The key controller attached to the help dialog.
 * @param keyval The pressed key value.
 * @param keycode The pressed hardware keycode.
 * @param state The active keyboard modifiers.
 * @param data Unused.
 *
 * @return Whether the key event was handled.
 */
static gboolean close_help_window_on_escape(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    if (keyval != GDK_KEY_Escape) {
        return FALSE;
    }

    gtk_window_close(GTK_WINDOW(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller))));
    return TRUE;
}

/**
 * Builds the help window
 *
 * @param data The LibreSplit GTK Application
 * @return Whether or not to remove this function from the queue
 */
static gboolean build_help_dialog(gpointer data)
{
    // Show already open window if another one is called.
    if (help_window_singleton) {
        gtk_window_present(help_window_singleton);
        return G_SOURCE_REMOVE;
    }

    LOG_DEBUG("Opening Help Window...");
    GtkApplication* app = GTK_APPLICATION(data);
    LSAppWindow* win = ls_get_main_app_window(app);
    if (win == NULL) {
        LOG_ERR("Main application window was not found");
        return G_SOURCE_REMOVE;
    }

    GtkWindow* parent = GTK_WINDOW(win);
    GtkIconTheme* theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(GTK_WIDGET(parent)));
    if (!gtk_icon_theme_has_icon(theme, "libresplit")) {
        LOG_ERR("The libresplit icon was not found");
        ls_alert_warning(parent, "LibreSplit", "Unable to open the help window", "Please report this warning");
        return G_SOURCE_REMOVE;
    }

    dialog_count_inc();

    GtkWindow* window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, "About LibreSplit");
    gtk_window_set_application(window, app);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_default_size(window, 400, -1);
    gtk_window_set_transient_for(window, parent);
    gtk_window_set_destroy_with_parent(window, TRUE);

    help_window_singleton = window;
    g_signal_connect(window, "destroy", G_CALLBACK(on_help_window_destroy), NULL);

    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(close_help_window_on_escape), NULL);
    gtk_widget_add_controller(GTK_WIDGET(window), key_controller);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, DIALOG_CONTENT_SPACING);
    gtk_widget_set_margin_top(content, DIALOG_MARGIN);
    gtk_widget_set_margin_bottom(content, DIALOG_MARGIN);
    gtk_widget_set_margin_start(content, DIALOG_MARGIN);
    gtk_widget_set_margin_end(content, DIALOG_MARGIN);
    gtk_widget_set_size_request(content, DIALOG_MIN_WIDTH, -1);
    gtk_window_set_child(window, content);

    GtkWidget* img = gtk_image_new_from_icon_name("libresplit");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 128);
    gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(content), img);

    GtkWidget* name_label = gtk_label_new("LibreSplit");
    gtk_widget_add_css_class(name_label, "heading");
    gtk_widget_set_halign(name_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(content), name_label);

    GtkWidget* description_label = gtk_label_new("A urn-based timer with autosplitter capabilities.");
    gtk_label_set_justify(GTK_LABEL(description_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(description_label), TRUE);
    gtk_widget_set_halign(description_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(content), description_label);

    char version_text[128];
    snprintf(version_text, sizeof(version_text), "Version %s", APP_VERSION);
    GtkWidget* version_label = gtk_label_new(version_text);
    gtk_widget_add_css_class(version_label, "dim-label");
    gtk_widget_set_halign(version_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(content), version_label);

    GtkWidget* links = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(links, 4);
    gtk_box_append(GTK_BOX(content), links);

    GtkWidget* website_lnk = gtk_link_button_new_with_label("https://libresplit.org/", "Check out our website!");
    gtk_widget_set_halign(website_lnk, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(links), website_lnk);
    GtkWidget* discord_lnk = gtk_link_button_new_with_label("https://discord.gg/qbzD7MBjyw", "Join Our Discord!");
    gtk_widget_set_halign(discord_lnk, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(links), discord_lnk);
    GtkWidget* github_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit", "Check out the source code");
    gtk_widget_set_halign(github_lnk, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(links), github_lnk);
    GtkWidget* resources_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit-resources", "Check out themes, autosplitters and splitfiles!");
    gtk_widget_set_halign(resources_lnk, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(links), resources_lnk);
    GtkWidget* wiki_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit/wiki", "Check our Wiki");
    gtk_widget_set_halign(wiki_lnk, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(links), wiki_lnk);

    gtk_window_present(window);
    return G_SOURCE_REMOVE;
}

/**
 * Action recalled by the context menu to show the help dialog.
 *
 * @param action Unused
 * @param parameter unused
 * @param app The LibreSplit GTK app
 */
void show_help_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    g_idle_add_full(G_PRIORITY_DEFAULT, build_help_dialog, g_object_ref(app), g_object_unref);
}
