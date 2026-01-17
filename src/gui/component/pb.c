/** \file pb.c
 *
 * Implementation of the "Personal Best" component
 */
#include "components.h"

/**
 * @brief The component representing a personal best
 */
typedef struct LSPb {
    LSComponent base; /*!< The base struct that is extended */
    GtkWidget* container; /*!< The container for the PB */
    GtkWidget* personal_best; /*< The actual personal best label */
} LSPb;
extern LSComponentOps ls_pb_operations;

#define PERSONAL_BEST "Personal best"

/**
 * Constructor
 */
LSComponent* ls_component_pb_new()
{
    LSPb* self;
    GtkWidget* label;

    self = malloc(sizeof(LSPb));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_pb_operations;

    self->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->container, "footer"); /* hack */
    add_class(self->container, "personal-best-container");
    gtk_widget_show(self->container);

    label = gtk_label_new(PERSONAL_BEST);
    add_class(label, "personal-best-label");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_container_add(GTK_CONTAINER(self->container), label);
    gtk_widget_show(label);

    self->personal_best = gtk_label_new(NULL);
    add_class(self->personal_best, "personal-best");
    gtk_widget_set_halign(self->personal_best, GTK_ALIGN_END);
    gtk_container_add(GTK_CONTAINER(self->container), self->personal_best);
    gtk_widget_show(self->personal_best);

    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void pb_delete(LSComponent* self)
{
    free(self);
}

/**
 * Returns the Personal Best GTK widget.
 *
 * @param self The personal best component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* pb_widget(LSComponent* self)
{
    return ((LSPb*)self)->container;
}

/**
 * Function to execute when ls_app_window_show_game is executed.
 *
 * @param self_ The personal best component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void pb_show_game(LSComponent* self_,
    const ls_game* game, const ls_timer* timer)
{
    LSPb* self = (LSPb*)self_;
    char str[256];
    if (game->split_count && game->split_times[game->split_count - 1]) {
        ls_time_string(
            str, game->split_times[game->split_count - 1]);
        gtk_label_set_text(GTK_LABEL(self->personal_best), str);
    }
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The best time component itself.
 */
static void pb_clear_game(LSComponent* self_)
{
    LSPb* self = (LSPb*)self_;
    gtk_label_set_text(GTK_LABEL(self->personal_best), "");
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The best time component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void pb_draw(LSComponent* self_, const ls_game* game,
    const ls_timer* timer)
{
    LSPb* self = (LSPb*)self_;
    char str[256];
    remove_class(self->personal_best, "time");
    gtk_label_set_text(GTK_LABEL(self->personal_best), "-");
    if (timer->curr_split == game->split_count
        && timer->split_times[game->split_count - 1]
        && (!game->split_times[game->split_count - 1]
            || (timer->split_times[game->split_count - 1]
                < game->split_times[game->split_count - 1]))) {
        add_class(self->personal_best, "time");
        ls_time_string(
            str, timer->split_times[game->split_count - 1]);
        gtk_label_set_text(GTK_LABEL(self->personal_best), str);
    } else if (game->split_times[game->split_count - 1]) {
        add_class(self->personal_best, "time");
        ls_time_string(
            str, game->split_times[game->split_count - 1]);
        gtk_label_set_text(GTK_LABEL(self->personal_best), str);
    }
}

LSComponentOps ls_pb_operations = {
    .delete = pb_delete,
    .widget = pb_widget,
    .show_game = pb_show_game,
    .clear_game = pb_clear_game,
    .draw = pb_draw
};
