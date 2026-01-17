#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include <ctype.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "../../timer.h"
#include "../utils.h"

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
    void (*cancel_run)(LSComponent* self, ls_timer* timer);
} LSComponentOps;

typedef struct LSComponentAvailable {
    char* name;
    LSComponent* (*new)();
} LSComponentAvailable;

// A NULL-terminated array of all available components
extern LSComponentAvailable ls_components[];

#endif /* __COMPONENTS_H__ */
