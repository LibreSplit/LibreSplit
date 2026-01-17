/** \file best-sum.c
 *
 * Implementation of the "Sum of best segments" component
 */
#include "components.h"

/**
 * @brief The component representing a sum of the best segments.
 */
typedef struct LSBestSum {
    LSComponent base; /*!< The base struct that is extended */
    GtkWidget* container; /*!< The container for the sum of bests */
    GtkWidget* sum_of_bests; /*!< The actual timer/label showing the sum of bests */
} LSBestSum;
extern LSComponentOps ls_best_sum_operations;

#define SUM_OF_BEST_SEGMENTS "Sum of best segments"

/**
 * Constructor
 */
LSComponent* ls_component_best_sum_new()
{
    LSBestSum* self;
    GtkWidget* label;

    self = malloc(sizeof(LSBestSum));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_best_sum_operations;

    self->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->container, "footer"); /* hack */
    add_class(self->container, "sum-of-bests-container");
    gtk_widget_show(self->container);

    label = gtk_label_new(SUM_OF_BEST_SEGMENTS);
    add_class(label, "sum-of-bests-label");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_container_add(GTK_CONTAINER(self->container), label);
    gtk_widget_show(label);

    self->sum_of_bests = gtk_label_new(NULL);
    add_class(self->sum_of_bests, "sum-of-bests");
    gtk_widget_set_halign(self->sum_of_bests, GTK_ALIGN_END);
    gtk_container_add(GTK_CONTAINER(self->container), self->sum_of_bests);
    gtk_widget_show(self->sum_of_bests);

    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void best_sum_delete(LSComponent* self)
{
    free(self);
}

/**
 * Returns the best sum GTK widget.
 *
 * @param self The best sum component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* best_sum_widget(LSComponent* self)
{
    return ((LSBestSum*)self)->container;
}

/**
 * Function to execute when ls_app_window_show_game is executed.
 *
 * @param self_ The best sum component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void best_sum_show_game(LSComponent* self_,
    const ls_game* game, const ls_timer* timer)
{
    LSBestSum* self = (LSBestSum*)self_;
    char str[256];
    if (game->split_count && timer->sum_of_bests) {
        ls_time_string(str, timer->sum_of_bests);
        gtk_label_set_text(GTK_LABEL(self->sum_of_bests), str);
    }
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The best time component itself.
 */
static void best_sum_clear_game(LSComponent* self_)
{
    LSBestSum* self = (LSBestSum*)self_;
    gtk_label_set_text(GTK_LABEL(self->sum_of_bests), "");
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The best time component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void best_sum_draw(LSComponent* self_, const ls_game* game,
    const ls_timer* timer)
{
    LSBestSum* self = (LSBestSum*)self_;
    char str[256];
    remove_class(self->sum_of_bests, "time");
    gtk_label_set_text(GTK_LABEL(self->sum_of_bests), "-");
    if (timer->sum_of_bests) {
        add_class(self->sum_of_bests, "time");
        ls_time_string(str, timer->sum_of_bests);
        gtk_label_set_text(GTK_LABEL(self->sum_of_bests), str);
    }
}

LSComponentOps ls_best_sum_operations = {
    .delete = best_sum_delete,
    .widget = best_sum_widget,
    .show_game = best_sum_show_game,
    .clear_game = best_sum_clear_game,
    .draw = best_sum_draw
};
