#include "gui/help_dialog.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "logging.h"
#include <gtk/gtk.h>
#include <stdio.h>

static GtkWidget* help_window_singleton = NULL;

/**
 * Help window destructor.
 *
 * @param widget The GTK Help Window;
 * @param event The Destruction event
 * @param user_data Unused.
 */
static gboolean on_help_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
    LOG_DEBUG("Destroying Help Window...");
    help_window_singleton = NULL;
    gtk_widget_destroy(widget);
    return TRUE;
}

/**
 * Builds the help window
 *
 * @param app The LibreSplit GTK Application
 * @param data Unused
 */
static void build_help_dialog(GtkApplication* app, gpointer data)
{
    LOG_DEBUG("Opening Help Window...");
    // Show already open window if another one is called.
    if (help_window_singleton) {
        gtk_window_present(GTK_WINDOW(help_window_singleton));
        return;
    }

    GtkWindow* parent = gtk_application_get_active_window(app);
    GtkWidget* window = gtk_dialog_new_with_buttons("About LibreSplit", parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL, NULL);
    gtk_window_set_application(GTK_WINDOW(window), app);
    help_window_singleton = window;
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 320);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(window, "delete-event", G_CALLBACK(on_help_window_delete), NULL);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_vexpand(box, TRUE);

    GtkIconTheme* theme = gtk_icon_theme_get_default();
    GError* err = NULL;

    GdkPixbuf* pixbuf = gtk_icon_theme_load_icon(theme, "libresplit", 200, 0, &err);
    if (!pixbuf) {
        LOG_WARNF("Icon load failed: %s", err ? err->message : "unknown error");
        if (err)
            g_error_free(err);
        return;
    }

    GtkWidget* img = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(img, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(img, 10, 10);
    gtk_container_add(GTK_CONTAINER(box), img);

    GtkWidget* label = gtk_label_new("LibreSplit\nA urn-based timer with autosplitter capabilities.");
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(box), label);

    char version_text[128];
    snprintf(version_text, sizeof(version_text), "Version %s", APP_VERSION);
    GtkWidget* version_label = gtk_label_new(version_text);
    gtk_widget_set_halign(version_label, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(box), version_label);

    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(window))), box);

    GtkWidget* website_lnk = gtk_link_button_new_with_label("https://libresplit.org/", "Check out our website!");
    gtk_container_add(GTK_CONTAINER(box), website_lnk);
    GtkWidget* discord_lnk = gtk_link_button_new_with_label("https://discord.gg/qbzD7MBjyw", "Join Our Discord!");
    gtk_container_add(GTK_CONTAINER(box), discord_lnk);
    GtkWidget* github_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit", "Check out the source code");
    gtk_container_add(GTK_CONTAINER(box), github_lnk);
    GtkWidget* resources_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit-resources", "Check out themes, autosplitters and splitfiles!");
    gtk_container_add(GTK_CONTAINER(box), resources_lnk);
    GtkWidget* wiki_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit/wiki", "Check our Wiki");
    gtk_container_add(GTK_CONTAINER(box), wiki_lnk);

    gtk_widget_show_all(window);
    gtk_window_present(GTK_WINDOW(window));
}

/**
 * Action recalled by the context menu to show the help dialog.
 *
 * @param action Unused
 * @param parameter The LibreSplit GTK app (if not NULL)
 * @param app The LibreSplit GTK app (fallback)
 */
void show_help_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    if (parameter != NULL) {
        app = parameter;
    }
    build_help_dialog(app, NULL);
}
