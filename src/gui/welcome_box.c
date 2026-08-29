#include "welcome_box.h"
#include "utils.h"
#include <gtk/gtk.h>
#include <stdlib.h>

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
    GtkIconTheme* theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(container));
    if (!gtk_icon_theme_has_icon(theme, "libresplit")) {
        g_printerr("Icon load failed: icon not found\n");
    } else {
        self->img = gtk_image_new_from_icon_name("libresplit");
        gtk_image_set_pixel_size(GTK_IMAGE(self->img), 200);
        gtk_widget_set_halign(self->img, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(self->img, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(self->img, 100, 100);
        gtk_box_append(GTK_BOX(self->box), self->img);
    }

    self->welcome_lbl = gtk_label_new("Welcome to LibreSplit!\nNo split is currently loaded.\nRight click this window to open a split JSON file!");
    gtk_box_append(GTK_BOX(self->box), self->welcome_lbl);
    gtk_box_append(GTK_BOX(container), self->box);
    return self;
}

void welcome_box_destroy(LSWelcomeBox* self)
{
    free(self);
}
