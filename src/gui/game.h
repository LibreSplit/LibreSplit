#pragma once

#include "gui/app_window.h"

void ls_app_window_clear_game(LSAppWindow* win);
void ls_app_window_show_game(LSAppWindow* win);
bool is_saving(void);
bool get_last_game_save_result(void);
bool get_last_runs_save_result(void);
void save_game(ls_game* game);
void save_game_join(bool exiting);
void timer_start(LSAppWindow* win);
