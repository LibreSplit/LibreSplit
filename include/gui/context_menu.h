#pragma once

#include <gtk/gtk.h>

void button_left_click(GtkWidget* widget, GdkEventButton* event);
void button_right_click(GdkEventButton* event, gpointer app);
gboolean handle_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer app);
gboolean handle_pointer_motion(GtkWidget* widget, GdkEventMotion* event, gpointer data);
