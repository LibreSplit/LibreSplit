#include "keybinds_callbacks.h"
#include "bind.h"
#include "src/gui/timer.h"

void keybind_start_split(GtkWidget* widget, LSAppWindow* win)
{
    timer_start_split(win);
}

void keybind_stop_reset(const char* str, LSAppWindow* win)
{
    // NOTE: [Penaz] [2026-02-02] This needs to be put as a "delayed handler",
    // ^ since it shows a dialog, such dialog would stop the event processing,
    // ^ locking up LibreSplit or potentially the entire DE when global_hotkeys is enabled.
    win->delayed_handlers.stop_reset = true;
}

void keybind_cancel(const char* str, LSAppWindow* win)
{
    timer_cancel_run(win);
}

void keybind_skip(const char* str, LSAppWindow* win)
{
    timer_skip(win);
}

void keybind_unsplit(const char* str, LSAppWindow* win)
{
    timer_unsplit(win);
}

void keybind_toggle_decorations(const char* str, LSAppWindow* win)
{
    toggle_decorations(win);
}

void keybind_toggle_win_on_top(const char* str, LSAppWindow* win)
{
    toggle_win_on_top(win);
}

/**
 * Matches a Gdk key press event with a Keybind.
 *
 * @param kb The keybind to compare against.
 * @param key The Gdk event key that needs to be compared.
 *
 * @return Zero if the keybinds don't match, a non-zero value otherwise.
 */
static int keybind_match(Keybind kb, GdkEventKey key)
{
    return key.keyval == kb.key && kb.mods == (key.state & gtk_accelerator_get_default_mod_mask());
}

gboolean ls_app_window_keypress(GtkWidget* widget,
    GdkEvent* event,
    gpointer data)
{
    LSAppWindow* win = (LSAppWindow*)data;
    if (keybind_match(win->keybinds.start_split, event->key)) {
        timer_start_split(win);
    } else if (keybind_match(win->keybinds.stop_reset, event->key)) {
        timer_stop_or_reset(win);
    } else if (keybind_match(win->keybinds.cancel, event->key)) {
        timer_cancel_run(win);
    } else if (keybind_match(win->keybinds.unsplit, event->key)) {
        timer_unsplit(win);
    } else if (keybind_match(win->keybinds.skip_split, event->key)) {
        timer_skip(win);
    } else if (keybind_match(win->keybinds.toggle_decorations, event->key)) {
        toggle_decorations(win);
    } else if (keybind_match(win->keybinds.toggle_win_on_top, event->key)) {
        toggle_win_on_top(win);
    }
    return TRUE;
}

void bind_global_hotkeys(AppConfig cfg, LSAppWindow* win)
{
    keybinder_init();
    keybinder_bind(
        cfg.keybinds.start_split.value.s,
        (KeybinderHandler)keybind_start_split,
        win);
    keybinder_bind(
        cfg.keybinds.stop_reset.value.s,
        (KeybinderHandler)keybind_stop_reset,
        win);
    keybinder_bind(
        cfg.keybinds.cancel.value.s,
        (KeybinderHandler)keybind_cancel,
        win);
    keybinder_bind(
        cfg.keybinds.unsplit.value.s,
        (KeybinderHandler)keybind_unsplit,
        win);
    keybinder_bind(
        cfg.keybinds.skip_split.value.s,
        (KeybinderHandler)keybind_skip,
        win);
    keybinder_bind(
        cfg.keybinds.toggle_decorations.value.s,
        (KeybinderHandler)keybind_toggle_decorations,
        win);
    keybinder_bind(
        cfg.keybinds.toggle_win_on_top.value.s,
        (KeybinderHandler)keybind_toggle_win_on_top,
        win);
}
