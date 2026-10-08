/** \file components.c
 *
 * Available Components and related utilities
 */
#include "components.h"

LSComponent* ls_component_title_new(void);
LSComponent* ls_component_splits_new(void);
LSComponent* ls_component_timer_new(void);
LSComponent* ls_component_detailed_timer_new(void);
LSComponent* ls_component_prev_segment_new(void);
LSComponent* ls_component_best_sum_new(void);
LSComponent* ls_component_pb_new(void);
LSComponent* ls_component_wr_new(void);

LSComponentAvailable ls_components[] = {
    { "title", ls_component_title_new },
    { "splits", ls_component_splits_new },
    { "timer", ls_component_timer_new },
    { "detailed-timer", ls_component_detailed_timer_new },
    { "prev-segment", ls_component_prev_segment_new },
    { "best-sum", ls_component_best_sum_new },
    { "pb", ls_component_pb_new },
    { "wr", ls_component_wr_new },
    { NULL, NULL }
};

/**
 * @brief Validates if the component name is a valid component that has been registered in `ls_components`
 *
 * @param name The name of the component.
 * @return bool Whether or not the component is valid.
 */
bool ls_component_is_valid(const char* name)
{
    // No string or empty string is not valid
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    // A loop over `ls_components` is fine, we could have 1000 components and O(1) would still be the same as O(n) realistically.
    for (size_t i = 0; i < G_N_ELEMENTS(ls_components) - 1; ++i) {
        // here we do ls_components count - 1 since we can ignore the null entry terminator as we have the actual count.
        if (strcmp(name, ls_components[i].name) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Gets the component entry from `ls_components` by the provided name
 * or NULL if the name does not exist.
 *
 * @param name The name of the component.
 * @return LSComponentAvailable* Pointer to the component if found or NULL.
 */
LSComponentAvailable* ls_component_get(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(ls_components) - 1; ++i) {
        if (strcmp(name, ls_components[i].name) == 0) {
            return &ls_components[i];
        }
    }

    return NULL;
}

/**
 * @brief Call the component's delete method when the components list is being destroyed.
 *
 * @param data The component to cleanup.
 */
void ls_component_destroy(gpointer data)
{
    LSComponent* component = data;
    if (component && component->ops && component->ops->delete) {
        component->ops->delete(component);
    }
}
