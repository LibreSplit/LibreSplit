#pragma once

#include <glib.h>
#include <stdbool.h>

gboolean display_non_capable_mem_read_dialog(gpointer data);

bool display_root_warning_dialog(void);

bool display_confirm_reset_dialog(void);
