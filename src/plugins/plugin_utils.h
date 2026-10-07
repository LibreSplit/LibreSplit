#pragma once
#include "gui/component/components.h"
#include "lasr/auto-splitter.h"
#include "lua.h"
#include "plugins/plugin_api.h"
#include "timer.h"
#include <stdint.h>

extern ExternalLASRFunctionRegistry external_lasr_functions;

extern TimerHookRegistry start_hooks;
extern TimerHookRegistry stop_hooks;
extern TimerHookRegistry split_hooks;
extern TimerHookRegistry reset_hooks;
extern TimerHookRegistry cancel_hooks;
extern TimerHookRegistry skip_hooks;
extern TimerHookRegistry unsplit_hooks;
extern TimerHookRegistry pause_hooks;
extern TimerHookRegistry unpause_hooks;

int register_lua_function(const char* name, lua_CFunction);

int register_event_hook(HookableEvent event, timer_hook_func fn);

int register_plugin_component(char* name, ls_component_new_func fn);

int init_external_lasr_functions(void);
