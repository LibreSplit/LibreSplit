#pragma once

#include "gtk/gtk.h"
#include <glib.h>
#include <stdbool.h>

gboolean display_non_capable_mem_read_dialog(gpointer data);
void show_theme_error_dialog(GtkWindow* parent_window, const char* theme_name, const char* theme_variant);
void show_split_theme_warning(GtkWindow* parent_window, const char* split_theme_name, const char* split_theme_variant);

bool display_confirm_reset_dialog();
