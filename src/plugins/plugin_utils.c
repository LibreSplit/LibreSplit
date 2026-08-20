#include "plugins/plugin_utils.h"
#include "logging.h"
#include "lua.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>

/**
 * Register a new C function to be added to the Lua Auto Splitter Runtime.
 *
 * @param[in] name The name the function should get inside the Lua Environment.
 * @param[in] fn The C function to be registered.
 *
 * @returns 0 if everything went okay, otherwise an error code.
 */
int register_lua_function(const char* name, lua_CFunction fn)
{
    LOG_DEBUGF("Pushing %s to the external Lua Function array", name);
    if (external_lasr_functions.count == external_lasr_functions.size) {
        LOG_DEBUG("Reallocating array for size");
        // Resize array if too small
        external_lasr_functions.size *= 2;
        // FIXME: [Penaz] [2026-08-20] Might lose the original reference if allocation fails
        external_lasr_functions.functions = realloc(external_lasr_functions.functions, external_lasr_functions.size * sizeof(struct lasr_function));
        if (!external_lasr_functions.functions) {
            LOG_ERR("Cannot reallocate external Lua C function array");
            free(external_lasr_functions.functions);
            // XXX: [Penaz] [2026-08-20] Maybe abort is a bit too much?
            abort();
        }
    }
    // Add the new function to the array
    //  FIXME: [Penaz] [2026-08-20] Check for NULL and empty function names, eventually for
    // ^ overwrites also
    external_lasr_functions.functions[external_lasr_functions.count].function_name = strdup(name);
    if (!external_lasr_functions.functions[external_lasr_functions.count].function_name) {
        LOG_ERRF("Cannot allocate memory for the function named %s", name);
        // XXX: [Penaz] [2026-08-20] Maybe abort is a bit too much?
        abort();
    }
    external_lasr_functions.functions[external_lasr_functions.count].function_ptr = fn;
    external_lasr_functions.count++;
    return 0;
}

/**
 * Utility function to push a function into the right registry.
 *
 * @param registry The registry to register the function on
 * @param fn The function to register
 *
 * @returns 0 if everything went well. An error code otherwise.
 */
static int push_function(TimerHookRegistry* registry, timer_hook_func fn)
{
    LOG_DEBUG("Pushing new function to hooks array");
    if (registry->count == registry->size) {
        LOG_DEBUG("Reallocating array for size");
        // Resize array
        registry->size *= 2;
        // FIXME: [Penaz] [2026-08-20] Might lose the original reference in case of failed allocation
        registry->functions = realloc(registry->functions, registry->size * sizeof(timer_hook_func));
        if (!registry->functions) {
            LOG_ERR("Cannot reallocate memory for timer hook.");
            // XXX: [Penaz] [2026-08-20] Maybe abort is a bit too much?
            abort();
        }
    }
    registry->functions[registry->count] = fn;
    registry->count++;
    return 0;
}

/**
 * Register a plugin function to be called when a certain event happens.o
 *
 * @param[in] event The event to react to.
 * @param[in] fn The function pointer to be called.
 *
 * @returns 0 if everything went okay, otherwise an error code.
 */
int register_event_hook(HookableEvent event, timer_hook_func fn)
{
    switch (event) {
        case START:
            LOG_DEBUG("Hooking new function into start event");
            push_function(&start_hooks, fn);
            break;
        case SPLIT:
            LOG_DEBUG("Hooking new function into split event");
            push_function(&split_hooks, fn);
            break;
        case STOP:
            LOG_DEBUG("Hooking new function into stop event");
            push_function(&stop_hooks, fn);
            break;
        case RESET:
            LOG_DEBUG("Hooking new function into reset event");
            push_function(&reset_hooks, fn);
            break;
        case CANCEL:
            LOG_DEBUG("Hooking new function into cancel event");
            push_function(&cancel_hooks, fn);
            break;
        case SKIP:
            LOG_DEBUG("Hooking new function into skip event");
            push_function(&skip_hooks, fn);
            break;
        case UNSPLIT:
            LOG_DEBUG("Hooking new function into unsplit event");
            push_function(&unsplit_hooks, fn);
            break;
        case PAUSE:
            LOG_DEBUG("Hooking new function into pause event");
            push_function(&pause_hooks, fn);
            break;
        case UNPAUSE:
            LOG_DEBUG("Hooking new function into unpause event");
            push_function(&unpause_hooks, fn);
            break;
        default:
            LOG_WARN("Tried to hook to nonexisting event, ignored.");
            return -1;
    }
    return 0;
}

/**
 * Initializes the external LASR functions array of pointers
 */
void init_external_lasr_functions(void)
{
    LOG_DEBUG("Initializing external LuaC functions array");
    // First array allocation
    external_lasr_functions.functions = malloc(external_lasr_functions.size * sizeof(struct lasr_function));
    if (external_lasr_functions.functions == NULL) {
        LOG_ERR("Cannot allocate memory for external LASR function pointers");
        // XXX: [Penaz] [2026-08-20] Maybe abort is a bit too much?
        abort();
    }
    external_lasr_functions.functions[0].function_name = NULL;
    external_lasr_functions.functions[0].function_ptr = NULL;
}
