#pragma once

#include <lua.h>
#include <sys/types.h>

pid_t find_process_by_name(const char* proc_name, int fl);
int find_process_id(lua_State* L);
