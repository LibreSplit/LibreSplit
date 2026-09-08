#pragma once

#include <gtk/gtk.h>

void open_activated(GSimpleAction* action, GVariant* parameter, gpointer app);

void save_activated(GSimpleAction* action, GVariant* parameter, gpointer app);

void reload_activated(GSimpleAction* action, GVariant* parameter, gpointer app);

void close_activated(GSimpleAction* action, GVariant* parameter, gpointer app);

void quit_activated(GSimpleAction* action, GVariant* parameter, gpointer app);

void toggle_auto_splitter(GSimpleAction* action, GVariant* value, gpointer user_data);

void menu_toggle_win_on_top(GSimpleAction* action, GVariant* value, gpointer app);

void open_auto_splitter(GSimpleAction* action, GVariant* parameter, gpointer app);
