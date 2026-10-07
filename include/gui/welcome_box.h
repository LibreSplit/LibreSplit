#pragma once

#include <gtk/gtk.h>

typedef struct {
    GtkWidget* box;
    GtkWidget* img;
    GtkWidget* welcome_lbl;
} LSWelcomeBox;

LSWelcomeBox* welcome_box_new(GtkWidget* container);
void welcome_box_destroy(LSWelcomeBox* self);
