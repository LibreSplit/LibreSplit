#include "delayed_handlers.h"
#include "delayed_callbacks.h"
#include "src/gui/app_window.h"

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
