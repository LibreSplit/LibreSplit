/** \file clock.c
 *
 * Implementation of the clock/timer component.
 */
#include "timer.h"

static const LSComponentSetting timer_settings[] = {
    { .key = "timing_method", .label = "Timing Method", .options = ls_timer_timing_method_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_COMPONENT_COMPARISON_DEFAULT } },
    { .key = "segment_timer", .label = "Segment Timer", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = false } },
    { .key = "digits_format", .label = "Digits Format", .options = ls_timer_digits_format_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIMER_DIGITS_SINGLE_SECONDS } },
    { .key = "accuracy", .label = "Accuracy", .options = ls_timer_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { .key = "gradient_colors", .label = "Gradient Colors", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = false } },
    { NULL }
};

/**
 * @brief The Timer component itself.
 */
typedef struct LSTimer {
    LSComponent base; /*!< The base struct that is extended. */
    GtkWidget* time; /*!< The timer container */
    GtkWidget* time_seconds; /*!< The label representing the seconds part of the timer */
    GtkWidget* time_millis; /*!< The label representing the fractional seconds part of the timer */
} LSTimer;
extern LSComponentOps ls_timer_operations;

/**
 * Constructor
 */
LSComponent* ls_component_timer_new(void)
{
    LSTimer* self;
    GtkWidget* spacer;

    self = malloc(sizeof(LSTimer));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_timer_operations;

    self->time = ls_timer_box_new();
    add_class(self->time, "timer");
    add_class(self->time, "time");

    spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(self->time), spacer);

    self->time_seconds = gtk_label_new(NULL);
    add_class(self->time_seconds, "timer-seconds");
    gtk_widget_set_valign(self->time_seconds, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(self->time), self->time_seconds);

    self->time_millis = gtk_label_new(NULL);
    add_class(self->time_millis, "timer-millis");
    gtk_widget_set_valign(self->time_millis, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(self->time), self->time_millis);

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
    ls_timer_set_text(self->time_seconds, self->time_millis, "", "");
    ls_timer_set_style(self->time, LS_TIMER_IDLE);
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
    bool segment_timer = ls_component_settings_get(self_, "segment_timer")->bool_val;
    LSTimerDigitsFormat digits_format = ls_component_settings_get(self_, "digits_format")->int_val;
    LSTimeAccuracy accuracy = ls_component_settings_get(self_, "accuracy")->int_val;
    ls_time_method method = ls_component_timer_method(self_, game->comparison_method);
    long long time = segment_timer ? ls_timer_get_segment_time(timer, method)
                                   : ls_time_get_by_method(ls_timer_get_time(timer, false), method);
    ls_timer_set_style(self->time, ls_timer_get_status(timer, method));
    ls_timer_string(str, millis, time, digits_format, accuracy);
    ls_timer_set_text(self->time_seconds, self->time_millis, str, millis);
    ls_timer_set_gradient(self->time, ls_component_settings_get(self_, "gradient_colors")->bool_val);
}

LSComponentOps ls_timer_operations = {
    .settings = timer_settings,
    .delete = ls_timer_delete,
    .widget = timer_widget,
    .clear_game = timer_clear_game,
    .draw = timer_draw
};
