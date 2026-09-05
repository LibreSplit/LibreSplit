#include "gui/app_window.h"
#include "settings/definitions.h"
#include <gtk/gtk.h>

extern void timer_start_split(LSAppWindow* win);
extern void timer_cancel_run(LSAppWindow* win);
extern void timer_skip(LSAppWindow* win);
extern void timer_unsplit(LSAppWindow* win);
extern void toggle_decorations(LSAppWindow* win);
extern void toggle_win_on_top(LSAppWindow* win);

gboolean ls_app_window_keypress(GtkWidget* widget, GdkEvent* event, gpointer data);

void keybind_start_split(GtkWidget* widget, LSAppWindow* win);

void keybind_stop_reset(const char* str, LSAppWindow* win);

void keybind_cancel(const char* str, LSAppWindow* win);

void keybind_skip(const char* str, LSAppWindow* win);

void keybind_unsplit(const char* str, LSAppWindow* win);

void keybind_toggle_decorations(const char* str, LSAppWindow* win);

void keybind_toggle_win_on_top(const char* str, LSAppWindow* win);

void bind_global_hotkeys(AppConfig cfg, LSAppWindow* win);
