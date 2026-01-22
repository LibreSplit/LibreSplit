#pragma once

#include "src/gui/app_window.h"

// Linker-provided symbols from fallback.o
extern const unsigned char _binary____src_fallback_css_start[];
extern const unsigned char _binary____src_fallback_css_end[];

int ls_app_window_find_theme(const LSAppWindow* win, const char* name, const char* variant, char* out_path);

void ls_app_load_theme_with_fallback(LSAppWindow* win, const char* name, const char* variant);
