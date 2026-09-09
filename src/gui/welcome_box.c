#include "gui/welcome_box.h"
#include "gui/utils.h"
#include <gtk/gtk.h>
#include <stdlib.h>

/**
 * Creates a welcome box to be shown on the main Window.
 * This appears when no game is loaded.
 *
 * @param container The container to put the welcome box in.
 *
 * @return A pointer to the welcome box itself.
 */
LSWelcomeBox* welcome_box_new(GtkWidget* container)
{
    LSWelcomeBox* self;
    self = malloc(sizeof(LSWelcomeBox));
    if (!self) {
        return NULL;
    }
    self->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->box, "welcome-screen");
    gtk_widget_set_margin_top(self->box, 8);
    gtk_widget_set_margin_bottom(self->box, 8);
    gtk_widget_set_margin_start(self->box, 8);
    gtk_widget_set_margin_end(self->box, 8);
    gtk_widget_set_vexpand(self->box, TRUE);
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    GError* err = NULL;
    GdkPixbuf* pixbuf = gtk_icon_theme_load_icon(theme, "libresplit", 200, 0, &err);
    if (!pixbuf) {
        g_printerr("Icon load failed: %s\n", err ? err->message : "unknown error");
        if (err)
            g_error_free(err);
    } else {
        self->img = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        gtk_widget_set_halign(self->img, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(self->img, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(self->img, 100, 100);
        gtk_container_add(GTK_CONTAINER(self->box), self->img);
    }

    self->welcome_lbl = gtk_label_new("Welcome to LibreSplit!\nNo split is currently loaded.\nRight click this window to open a split JSON file!");
    gtk_container_add(GTK_CONTAINER(self->box), self->welcome_lbl);
    gtk_container_add(GTK_CONTAINER(container), self->box);
    return self;
}

/**
 * Destructor for the welcome box.
 *
 * @param self The Welcome Box itself.
 */
void welcome_box_destroy(LSWelcomeBox* self)
{
    free(self);
}
