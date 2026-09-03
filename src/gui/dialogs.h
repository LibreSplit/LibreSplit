#pragma once

#include "src/gui/app_window.h"
#include "src/gui/widgets/dialog.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>

gboolean display_non_capable_mem_read_dialog(gpointer data);

int display_root_warning_dialog(void);

void display_confirm_reset_dialog(LSDialogCallback perform_reset, LSAppWindow* win);
