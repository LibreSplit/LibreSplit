/** \file components.c
 *
 * Available Components and related utilities
 */
#include "gui/component/components.h"
#include "logging.h"

LSComponent* ls_component_title_new(void);
LSComponent* ls_component_splits_new(void);
LSComponent* ls_component_timer_new(void);
LSComponent* ls_component_detailed_timer_new(void);
LSComponent* ls_component_prev_segment_new(void);
LSComponent* ls_component_best_sum_new(void);
LSComponent* ls_component_pb_new(void);
LSComponent* ls_component_wr_new(void);

LSComponentRegistry ls_components = {
    .count = 0,
    .size = 2,
    .components = NULL,
    .enabled = false,
};

/**
 * Initializes the GUI component registry.
 *
 * @returns true if everything went well, false otherwise.
 */
static bool initialize_component_registry(void)
{
    LOG_INFO("Initializing Component Registry");
    if (ls_components.enabled) {
        LOG_INFO("Components Registry already initialized");
        return true;
    }
    ls_components.components = malloc(ls_components.size * sizeof(LSComponentAvailable));
    if (!ls_components.components) {
        LOG_ERR("GUI Components Registry initialization failed (malloc failed).");
        // At this point, we have no components, which means a non-functioning timer. Abort.
        abort();
    }
    ls_components.enabled = true;
    return true;
}

/**
 * Registers a component to the component registry.
 *
 * @param name The name of the component
 * @param init_func The ls_component_*_new function used to initialize the component
 *
 * @returns True if the component registered successfully, false otherwise.
 */
bool register_component(char* name, ls_component_new_func init_func)
{
    if (!ls_components.enabled) {
        LOG_INFO("Components Registry not initialized, initializing...");
        initialize_component_registry();
    }
    if (ls_components.count >= ls_components.size) {
        size_t new_size = ls_components.size * 2;
        LSComponentAvailable* tmp_registry = realloc(ls_components.components, new_size * sizeof(LSComponentAvailable));
        if (!tmp_registry) {
            LOG_ERR("Cannot reallocate memory for components registry");
            return false;
        }
        ls_components.components = tmp_registry;
        ls_components.size = new_size;
    }
    ls_components.components[ls_components.count].name = name;
    ls_components.components[ls_components.count].new = init_func;
    ls_components.count++;
    return true;
}

/**
 * Initializes the default libresplit components.
 *
 * Might be a future hook point for customization.
 *
 * @returns True if the components initialized correctly (for now always).
 */
bool init_components(void)
{
    register_component("title", ls_component_title_new);
    register_component("splits", ls_component_splits_new);
    /*register_component("timer", ls_component_timer_new);*/
    register_component("detailed-timer", ls_component_detailed_timer_new);
    register_component("prev-segment", ls_component_prev_segment_new);
    register_component("best-sum", ls_component_best_sum_new);
    register_component("pb", ls_component_pb_new);
    register_component("wr", ls_component_wr_new);
    return 0;
}
