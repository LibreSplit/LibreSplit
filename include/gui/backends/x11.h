#pragma once

#include <gtk/gtk.h>

bool is_x11_display(void);
void x11_set_keep_above(GtkWindow* window, gboolean setting);
