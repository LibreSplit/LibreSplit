#pragma once

#include "src/gui/app_window.h"

void ls_app_window_clear_game(LSAppWindow* win);
void ls_app_window_show_game(LSAppWindow* win);
void save_game(ls_game* game);
void save_game_join(void);
void timer_start(LSAppWindow* win);
