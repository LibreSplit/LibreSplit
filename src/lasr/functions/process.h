#pragma once

#include <lua.h>
#include <stdbool.h>

int find_process_id(lua_State* L);
void set_process_selection(bool allowed);
bool attach_process(void);
void detach_process(void);
void clear_process(void);
