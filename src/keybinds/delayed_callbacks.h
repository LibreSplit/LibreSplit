#pragma once
#include "src/gui/app_window.h"

extern void timer_stop_or_reset(LSAppWindow* win);

void process_delayed_handlers(LSAppWindow* win);
