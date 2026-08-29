#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>

/** TODO: Remove temp helpers */
gint run_dialog(GtkDialog* dialog);

gboolean display_non_capable_mem_read_dialog(gpointer data);

bool display_root_warning_dialog(void);

bool display_confirm_reset_dialog(void);
