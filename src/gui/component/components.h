#pragma once

#include "gui/components/component.h"
#include "gui/utils.h"
#include "timer.h"
#include <gtk/gtk.h>

typedef struct LSComponentAvailable {
    char* name;
    LSComponent* (*new)(void);
} LSComponentAvailable;

typedef LSComponent* (*ls_component_new_func)(void);

typedef struct _ComponentRegistry {
    int count; /*!< Number of loaded components */
    int size; /*!< Size of the component registry */
    LSComponentAvailable* components; /*!< Array of the available components */
    bool enabled; /*!< Defines if the component registry is initialized and enabled */
} LSComponentRegistry;

extern LSComponentRegistry ls_components;

bool register_component(char*, ls_component_new_func);

bool init_components(void);
