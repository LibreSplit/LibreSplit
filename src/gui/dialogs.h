#pragma once

#include "src/gui/widgets/dialog.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>

void display_non_capable_mem_read_dialog();

int display_root_warning_dialog();

void display_confirm_reset_dialog(LSDialogCallback perform_reset, gpointer user_data, LSDialogCallback destroy_user_data);
