#pragma once

#include <gtk/gtk.h>

void button_right_click(GdkEventButton* event, gpointer app);
gboolean handle_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer app);
