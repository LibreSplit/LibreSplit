#include "help_dialog.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include <gtk/gtk.h>

static gboolean on_help_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
    gtk_widget_destroy(widget);
    return TRUE;
}

static void build_help_dialog(GtkApplication* app, gpointer data)
{
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "About LibreSplit");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 320);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(window, "delete-event", G_CALLBACK(on_help_window_delete), NULL);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_vexpand(box, TRUE);

    GError* err = NULL;
    GdkPixbuf* pixbuf = NULL;
    GdkPixbuf* scaled_pixbuf = NULL;

    pixbuf = gdk_pixbuf_new_from_file("assets/libresplit.svg", &err);
    if (!pixbuf) {
        g_printerr("Error loading SVG: %s\n", err ? err->message : "unknown error");
        if (err)
            g_error_free(err);
        return;
    }

    scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, 200, 200, GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);
    pixbuf = NULL;
    if (!scaled_pixbuf) {
        return;
    }

    GtkWidget* img = gtk_image_new_from_pixbuf(scaled_pixbuf);
    g_object_unref(scaled_pixbuf);
    scaled_pixbuf = NULL;

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

    gtk_container_add(GTK_CONTAINER(window), box);

    GtkWidget* website_lnk = gtk_link_button_new_with_label("https://libresplit.org/", "Check out our website!");
    gtk_container_add(GTK_CONTAINER(box), website_lnk);
    GtkWidget* discord_lnk = gtk_link_button_new_with_label("https://discord.gg/qbzD7MBjyw", "Join Our Discord!");
    gtk_container_add(GTK_CONTAINER(box), discord_lnk);
    GtkWidget* github_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit", "Check out the source code");
    gtk_container_add(GTK_CONTAINER(box), github_lnk);
    GtkWidget* wiki_lnk = gtk_link_button_new_with_label("https://github.com/LibreSplit/LibreSplit/wiki", "Check our Wiki");
    gtk_container_add(GTK_CONTAINER(box), wiki_lnk);

    gtk_widget_show_all(window);
    gtk_window_present(GTK_WINDOW(window));
}

void show_help_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    if (parameter != NULL) {
        app = parameter;
    }
    build_help_dialog(app, NULL);
}
