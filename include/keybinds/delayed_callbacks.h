#pragma once
#include "gui/app_window.h"

extern void timer_stop_or_reset(LSAppWindow* win);
extern void timer_cancel_run(LSAppWindow* win);

void process_delayed_handlers(LSAppWindow* win);
