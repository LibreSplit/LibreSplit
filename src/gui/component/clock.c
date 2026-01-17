/** \file clock.c
 *
 * Implementation of the clock/timer component.
 */
#include "components.h"

/**
 * @brief The Timer component itself.
 */
typedef struct LSTimer {
    LSComponent base; /*!< The base struct that is extended. */
    GtkWidget* time; /*!< The timer container */
    GtkWidget* time_seconds; /*!< The label representing the seconds part of the timer */
    GtkWidget* time_millis; /*!< The label representing the milliseconds part of the timer */
} LSTimer;
extern LSComponentOps ls_timer_operations;

/**
 * Constructor
 */
LSComponent* ls_component_timer_new()
{
    LSTimer* self;
    GtkWidget* spacer;

    self = malloc(sizeof(LSTimer));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_timer_operations;

    self->time = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->time, "timer");
    add_class(self->time, "time");
    gtk_widget_show(self->time);

    spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_container_add(GTK_CONTAINER(self->time), spacer);
    gtk_widget_show(spacer);

    self->time_seconds = gtk_label_new(NULL);
    add_class(self->time_seconds, "timer-seconds");
    gtk_widget_set_valign(self->time_seconds, GTK_ALIGN_BASELINE);
    gtk_container_add(GTK_CONTAINER(self->time), self->time_seconds);
    gtk_widget_show(self->time_seconds);

    spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(spacer, GTK_ALIGN_END);
    gtk_container_add(GTK_CONTAINER(self->time), spacer);
    gtk_widget_show(spacer);

    self->time_millis = gtk_label_new(NULL);
    add_class(self->time_millis, "timer-millis");
    gtk_widget_set_valign(self->time_millis, GTK_ALIGN_BASELINE);
    gtk_container_add(GTK_CONTAINER(spacer), self->time_millis);
    gtk_widget_show(self->time_millis);

    return (LSComponent*)self;
}

// Avoid collision with timer_delete of time.h
/**
 * Destructor.
 *
 * @param self The clock component itself
 */
static void ls_timer_delete(LSComponent* self)
{
    free(self);
}

/**
 * Returns the clock GTK widget.
 *
 * @param self The clock component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* timer_widget(LSComponent* self)
{
    return ((LSTimer*)self)->time;
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The best time component itself.
 */
static void timer_clear_game(LSComponent* self_)
{
    LSTimer* self = (LSTimer*)self_;
    gtk_label_set_text(GTK_LABEL(self->time_seconds), "");
    gtk_label_set_text(GTK_LABEL(self->time_millis), "");
    remove_class(self->time, "behind");
    remove_class(self->time, "losing");
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The best time component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void timer_draw(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    LSTimer* self = (LSTimer*)self_;
    char str[256], millis[256];
    int curr;

    curr = timer->curr_split;
    if (curr == game->split_count) {
        --curr;
    }

    remove_class(self->time, "delay");
    remove_class(self->time, "behind");
    remove_class(self->time, "losing");
    remove_class(self->time, "best-split");

    if (curr == game->split_count) {
        curr = game->split_count - 1;
    }
    if (timer->time <= 0) {
        add_class(self->time, "delay");
    } else {
        if (timer->curr_split == game->split_count
            && timer->split_info[curr]
                & LS_INFO_BEST_SPLIT) {
            add_class(self->time, "best-split");
        } else {
            if (timer->split_info[curr]
                & LS_INFO_BEHIND_TIME) {
                add_class(self->time, "behind");
            }
            if (timer->split_info[curr]
                & LS_INFO_LOSING_TIME) {
                add_class(self->time, "losing");
            }
        }
    }
    ls_time_millis_string(str, &millis[1], timer->time);
    millis[0] = '.';
    gtk_label_set_text(GTK_LABEL(self->time_seconds), str);
    gtk_label_set_text(GTK_LABEL(self->time_millis), millis);
}

LSComponentOps ls_timer_operations = {
    .delete = ls_timer_delete,
    .widget = timer_widget,
    .clear_game = timer_clear_game,
    .draw = timer_draw
};
