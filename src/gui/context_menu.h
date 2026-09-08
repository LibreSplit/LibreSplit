#pragma once

#include <gtk/gtk.h>

void button_left_click(GtkGestureClick* gesture, double x, double y);
void button_right_click(GtkGestureClick* gesture, double x, double y, gpointer app);
void handle_button_pressed(GtkGestureClick* gesture, int n_press, double x, double y, gpointer app);
void handle_pointer_motion(GtkEventControllerMotion* controller, double x, double y, gpointer data);
void handle_pointer_leave(GtkEventControllerMotion* controller, gpointer data);
