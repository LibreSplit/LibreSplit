#pragma once

#include <gtk/gtk.h>

// Forward declarations
typedef struct ls_timer ls_timer;
typedef struct ls_game ls_game;
typedef struct LSComponentOps LSComponentOps;

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
