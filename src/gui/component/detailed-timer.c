/** \file detailed-timer.c
 *
 * Implementation of the "Detailed timer" component.
 */
#include "timer.h"
#include <limits.h>

static const LSComponentSetting detailed_timer_settings[] = {
    { .key = "timing_method", .label = "Timing Method", .options = ls_timer_timing_method_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_COMPONENT_COMPARISON_DEFAULT } },
    { .key = "digits_format", .label = "Digits Format", .options = ls_timer_digits_format_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIMER_DIGITS_SINGLE_SECONDS } },
    { .key = "accuracy", .label = "Accuracy", .options = ls_timer_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { .key = "gradient_colors", .label = "Gradient Colors", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = false } },
    { NULL }
};

/**
 * @brief The component representing the detailed timer part of the window.
 */
typedef struct LSDetailedTimer {
    LSComponent base; /*!< The base struct that is extended */
    GtkWidget* detailed_timer; /*!< The container for the detailed timer */
    GtkWidget* detailed_info; /*!< Box */
    GtkWidget* segment_pb; /*!< Label */
    GtkWidget* segment_best; /*!< Label */
    GtkWidget* detailed_time; /*!< Box */
    GtkWidget* time; /*!< Box */
    GtkWidget* time_seconds; /*!< Label */
    GtkWidget* time_millis; /*!< Label */
    GtkWidget* segment; /*!< Box */
    GtkWidget* segment_seconds; /*!< Label */
    GtkWidget* segment_millis; /*!< Label */
} LSDetailedTimer;
extern LSComponentOps ls_detailed_timer_operations;

/**
 * Constructor
 */
LSComponent* ls_component_detailed_timer_new(json_t* settings)
{
    LSDetailedTimer* self;
    GtkWidget* spacer;

    self = malloc(sizeof(LSDetailedTimer));
    if (!self) {
        return NULL;
    }

    self->base.ops = &ls_detailed_timer_operations;
    ls_component_settings_load((LSComponent*)self, settings);

    self->detailed_timer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->detailed_timer, "timer-container");

    self->detailed_info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(self->detailed_info, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(self->detailed_timer), self->detailed_info);
    add_class(self->detailed_info, "detailed-timer");

    self->segment_best = gtk_label_new(NULL);
    add_class(self->segment_best, "segment-best");

    self->segment_pb = gtk_label_new(NULL);
    add_class(self->segment_pb, "segment-pb");
    gtk_box_append(GTK_BOX(self->detailed_info), self->segment_pb);
    gtk_box_append(GTK_BOX(self->detailed_info), self->segment_best);
    //
    self->detailed_time = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->detailed_time, "timer");
    gtk_widget_set_hexpand(self->detailed_time, TRUE);
    gtk_box_append(GTK_BOX(self->detailed_timer), self->detailed_time);

    self->time = ls_timer_box_new();
    add_class(self->time, "timer");
    add_class(self->time, "time");
    gtk_box_append(GTK_BOX(self->detailed_time), self->time);

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

    self->segment = ls_timer_box_new();
    add_class(self->segment, "segment");
    gtk_box_append(GTK_BOX(self->detailed_time), self->segment);

    spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(self->segment), spacer);

    self->segment_seconds = gtk_label_new(NULL);
    add_class(self->segment_seconds, "segment-seconds");
    gtk_widget_set_valign(self->segment_seconds, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(self->segment), self->segment_seconds);

    self->segment_millis = gtk_label_new(NULL);
    add_class(self->segment_millis, "segment-millis");
    gtk_widget_set_valign(self->segment_millis, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(self->segment), self->segment_millis);

    return (LSComponent*)self;
}

// Avoid collision with timer_delete of time.h
/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void ls_detailed_timer_delete(LSComponent* self)
{
    free(self);
}

/**
 * Returns the detailed timer GTK widget.
 *
 * @param self The detailed timer component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* detailed_timer_widget(LSComponent* self)
{
    return ((LSDetailedTimer*)self)->detailed_timer;
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The detailed timer component itself.
 */
static void detailed_timer_clear_game(LSComponent* self_)
{
    LSDetailedTimer* self = (LSDetailedTimer*)self_;
    ls_timer_set_text(self->time_seconds, self->time_millis, "", "");
    ls_timer_set_text(self->segment_seconds, self->segment_millis, "", "");

    gtk_label_set_text(GTK_LABEL(self->segment_pb), "");
    gtk_label_set_text(GTK_LABEL(self->segment_best), "");
    ls_timer_set_style(self->time, LS_TIMER_IDLE);
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The detailed timer component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void detailed_timer_draw(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    LSDetailedTimer* self = (LSDetailedTimer*)self_;
    char str[256], millis[10], seg[256], seg_millis[10];
    char pb[256] = "PB:    ";
    char best[256] = "Best: ";
    ls_time_method method = ls_component_timer_method(self_, game->comparison_method);
    LSTimerDigitsFormat digits_format = ls_component_settings_get(self_, "digits_format")->int_val;
    LSTimeAccuracy accuracy = ls_component_settings_get(self_, "accuracy")->int_val;

    unsigned int curr = timer->curr_split;
    if (curr && curr == game->split_count) {
        --curr;
    }

    ls_timer_status status = ls_timer_get_status(timer, method);
    ls_timer_set_style(self->time, status);
    ls_timer_string(str, millis, ls_time_get_by_method(ls_timer_get_time(timer, false), method), digits_format, accuracy);
    ls_timer_set_text(self->time_seconds, self->time_millis, str, millis);

    ls_timer_string(seg, seg_millis, ls_timer_get_segment_time(timer, method), digits_format, accuracy);
    ls_timer_set_text(self->segment_seconds, self->segment_millis, seg, seg_millis);
    bool gradient = ls_component_settings_get(self_, "gradient_colors")->bool_val;
    ls_timer_set_gradient(self->time, gradient);
    ls_timer_set_gradient(self->segment, gradient);

    ls_time_string(&pb[6], curr < game->split_count ? ls_time_get_by_method(game->segment_times[curr], method) : LLONG_MAX);
    gtk_label_set_text(GTK_LABEL(self->segment_pb), pb);

    ls_time_string(&best[6], curr < game->split_count ? ls_time_get_by_method(game->best_segments[curr], method) : LLONG_MAX);
    gtk_label_set_text(GTK_LABEL(self->segment_best), best);
}

LSComponentOps ls_detailed_timer_operations = {
    .settings = detailed_timer_settings,
    .delete = ls_detailed_timer_delete,
    .widget = detailed_timer_widget,
    .clear_game = detailed_timer_clear_game,
    .draw = detailed_timer_draw
};
