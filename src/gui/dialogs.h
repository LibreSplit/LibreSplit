#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>

/** TODO: Remove temp helpers */
gint run_dialog(GtkDialog* dialog);

void display_non_capable_mem_read_dialog();

int display_root_warning_dialog();

bool display_confirm_reset_dialog();
