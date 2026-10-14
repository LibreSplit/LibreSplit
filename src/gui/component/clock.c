/** \file clock.c
 *
 * Implementation of the clock/timer component.
 */
#include "clock.h"
#include <limits.h>

static const LSComponentOption timing_method_options[] = {
    { .key = LS_COMPONENT_COMPARISON_DEFAULT, .label = "Default" },
    { .key = LS_COMPONENT_COMPARISON_REAL_TIME, .label = "Real Time" },
    { .key = LS_COMPONENT_COMPARISON_GAME_TIME, .label = "Game Time" },
    { 0 }
};

static const LSComponentOption digits_format_options[] = {
    { .key = LS_TIMER_DIGITS_SINGLE_SECONDS, .label = "1" },
    { .key = LS_TIMER_DIGITS_DOUBLE_SECONDS, .label = "01" },
    { .key = LS_TIMER_DIGITS_SINGLE_MINUTES, .label = "0:01" },
    { .key = LS_TIMER_DIGITS_DOUBLE_MINUTES, .label = "00:01" },
    { .key = LS_TIMER_DIGITS_SINGLE_HOURS, .label = "0:00:01" },
    { .key = LS_TIMER_DIGITS_DOUBLE_HOURS, .label = "00:00:01" },
    { 0 }
};

static const LSComponentOption time_accuracy_options[] = {
    { .key = LS_TIME_ACCURACY_DEFAULT, .label = "Default" },
    { .key = LS_TIME_ACCURACY_SECONDS, .label = "Seconds" },
    { .key = LS_TIME_ACCURACY_TENTHS, .label = "Tenths" },
    { .key = LS_TIME_ACCURACY_HUNDREDTHS, .label = "Hundredths" },
    { .key = LS_TIME_ACCURACY_MILLISECONDS, .label = "Milliseconds" },
    { 0 }
};

static const LSComponentSetting timer_settings[] = {
    { .key = "timing_method", .label = "Timing Method", .options = timing_method_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_COMPONENT_COMPARISON_DEFAULT } },
    { .key = "segment_timer", .label = "Segment Timer", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = false } },
    { .key = "digits_format", .label = "Digits Format", .options = digits_format_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIMER_DIGITS_SINGLE_SECONDS } },
    { .key = "accuracy", .label = "Accuracy", .options = time_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { NULL }
};

void ls_timer_string(char* string, char* fraction, long long time, LSTimerDigitsFormat digits_format, LSTimeAccuracy accuracy)
{
    fraction[0] = '\0';
    if (time == LLONG_MAX) {
        strcpy(string, "-");
        return;
    }

    const char* sign = time < 0 ? "-" : "";
    unsigned long long magnitude = time < 0 ? 0ULL - (unsigned long long)time : (unsigned long long)time;
    unsigned long long hours = magnitude / 3600000000ULL;
    unsigned int minutes = (magnitude / 60000000ULL) % 60;
    unsigned int seconds = (magnitude / 1000000ULL) % 60;

    if (hours || digits_format >= LS_TIMER_DIGITS_SINGLE_HOURS) {
        sprintf(string, "%s%0*llu:%02u:%02u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_HOURS ? 2 : 1, hours, minutes, seconds);
    } else if (minutes || digits_format >= LS_TIMER_DIGITS_SINGLE_MINUTES) {
        sprintf(string, "%s%0*u:%02u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_MINUTES ? 2 : 1, minutes, seconds);
    } else {
        sprintf(string, "%s%0*u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_SECONDS ? 2 : 1, seconds);
    }

    int decimals = ls_component_time_accuracy_decimals(accuracy);
    if (decimals > 0) {
        char subseconds[7];
        sprintf(subseconds, "%06llu", magnitude % 1000000ULL);
        sprintf(fraction, ".%.*s", decimals, subseconds);
    }
}

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

    self->time = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->time, "timer");
    add_class(self->time, "time");

    spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(self->time), spacer);

    self->time_seconds = gtk_label_new(NULL);
    add_class(self->time_seconds, "timer-seconds");
    gtk_widget_set_valign(self->time_seconds, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(self->time), self->time_seconds);

    spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(spacer, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(self->time), spacer);

    self->time_millis = gtk_label_new(NULL);
    add_class(self->time_millis, "timer-millis");
    gtk_widget_set_valign(self->time_millis, GTK_ALIGN_BASELINE_FILL);
    gtk_box_append(GTK_BOX(spacer), self->time_millis);

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
    LSComponentComparisonMethod timing_method = ls_component_settings_get(self_, "timing_method")->int_val;
    bool segment_timer = ls_component_settings_get(self_, "segment_timer")->bool_val;
    LSTimerDigitsFormat digits_format = ls_component_settings_get(self_, "digits_format")->int_val;
    LSTimeAccuracy accuracy = ls_component_settings_get(self_, "accuracy")->int_val;
    ls_time_method method = game->comparison_method;
    switch (timing_method) {
        case LS_COMPONENT_COMPARISON_DEFAULT:
            break;
        case LS_COMPONENT_COMPARISON_REAL_TIME:
            method = LS_REAL_TIME;
            break;
        case LS_COMPONENT_COMPARISON_GAME_TIME:
            method = LS_GAME_TIME;
            break;
    }

    unsigned int curr = timer->curr_split;
    if (curr && curr == game->split_count) {
        --curr;
    }

    long long time = ls_time_get_by_method(ls_timer_get_time(timer, false), method);
    if (segment_timer && curr) {
        long long previous = ls_time_get_by_method(timer->split_times[curr - 1], method);
        bool zero_game_time = method == LS_GAME_TIME && previous == 0 && is_time_valid(timer->split_times[curr - 1].real_time);
        if (!zero_game_time) {
            time = ls_segment_value(time, previous, false);
        }
    }

    remove_class(self->time, "delay");
    remove_class(self->time, "behind");
    remove_class(self->time, "losing");
    remove_class(self->time, "best-split");

    if (curr && curr == game->split_count) {
        curr = game->split_count - 1;
    }
    if (ls_time_get_by_method(ls_timer_get_time(timer, false), game->comparison_method) <= 0) {
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
    ls_timer_string(str, millis, time, digits_format, accuracy);
    gtk_label_set_text(GTK_LABEL(self->time_seconds), str);
    gtk_label_set_text(GTK_LABEL(self->time_millis), millis);
}

LSComponentOps ls_timer_operations = {
    .settings = timer_settings,
    .delete = ls_timer_delete,
    .widget = timer_widget,
    .clear_game = timer_clear_game,
    .draw = timer_draw
};
