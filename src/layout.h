#pragma once

#include "src/gui/app_window.h"
#include <gio/gio.h>
#include <jansson.h>
#include <stdbool.h>

bool ls_load_default_layout(LSAppWindow* win);
bool ls_load_layout(LSAppWindow* win, const char* path);
