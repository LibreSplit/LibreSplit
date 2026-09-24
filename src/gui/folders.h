#pragma once
#include <gio/gio.h>
#include <glib.h>

void launch_fm_splits(GSimpleAction* action, GVariant* parameter, gpointer app);
void launch_fm_autosplitters(GSimpleAction* action, GVariant* parameter, gpointer app);
void launch_fm_themes(GSimpleAction* action, GVariant* parameter, gpointer app);
void launch_fm_logs(GSimpleAction* action, GVariant* parameter, gpointer app);
