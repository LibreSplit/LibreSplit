#include "delayed_handlers.h"
#include "delayed_callbacks.h"
#include "src/gui/app_window.h"

void process_delayed_handlers(LSAppWindow* win)
{
    if (win->delayed_handlers.stop_reset) {
        timer_stop_or_reset(win);
        win->delayed_handlers.stop_reset = false;
    }
}
