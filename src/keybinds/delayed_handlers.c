#include "delayed_handlers.h"
#include "delayed_callbacks.h"
#include "src/gui/app_window.h"

/**
 * @brief Process all handlers that are pending to be processed.
 *
 * This is usually done for all those keybind handlers that can't return
 * immediately (so we just set a boolean in a struct to be worked on later)
 * or they would lock up the entire Desktop Environment in case global
 * hotkeys are active (cause a key handler would never exit.)
 *
 * @param win The Main LibreSplit Window.
 */
void process_delayed_handlers(LSAppWindow* win)
{
    if (win->delayed_handlers.stop_reset) {
        win->delayed_handlers.stop_reset = false;
        timer_stop_or_reset(win);
    }

    if (win->delayed_handlers.cancel) {
        win->delayed_handlers.cancel = false;
        timer_cancel_run(win);
    }
}
