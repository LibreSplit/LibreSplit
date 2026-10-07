#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include <ctype.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "gui/utils.h"
#include "timer.h"

typedef struct LSComponentOps LSComponentOps; // forward declaration

typedef struct LSComponent {
    LSComponentOps* ops;
} LSComponent;

typedef struct LSComponentOps {
    void (*delete)(LSComponent* self);
    GtkWidget* (*widget)(LSComponent* self);

    void (*resize)(LSComponent* self, int win_width, int win_height);
    void (*show_game)(LSComponent* self, const ls_game* game, const ls_timer* timer);
    void (*clear_game)(LSComponent* self);
    void (*draw)(LSComponent* self, const ls_game* game, const ls_timer* timer);

    void (*start_split)(LSComponent* self, const ls_timer* timer);
    void (*skip)(LSComponent* self, const ls_timer* timer);
    void (*unsplit)(LSComponent* self, const ls_timer* timer);
    void (*stop_reset)(LSComponent* self, ls_timer* timer);
    void (*pause)(LSComponent* self, ls_timer* timer);
    void (*unpause)(LSComponent* self, ls_timer* timer);
    void (*cancel_run)(LSComponent* self, ls_timer* timer);
} LSComponentOps;

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

#endif /* __COMPONENTS_H__ */
