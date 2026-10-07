#pragma once
#include <gio/gio.h>
#include <glib.h>

#define FOLDERS_SPLITS_DIR "splits"
#define FOLDERS_AUTO_SPLITTERS_DIR "auto-splitters"
#define FOLDERS_THEMES_DIR "themes"
// TODO: Define a logs directory when it moves out of the root data path
#define FOLDERS_LOGS_DIR ""

void launch_fm_dir(GSimpleAction* action, GVariant* parameter, gpointer app);
