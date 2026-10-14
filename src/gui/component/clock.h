#pragma once

#include "components.h"

typedef enum LSTimerDigitsFormat {
    LS_TIMER_DIGITS_SINGLE_SECONDS,
    LS_TIMER_DIGITS_DOUBLE_SECONDS,
    LS_TIMER_DIGITS_SINGLE_MINUTES,
    LS_TIMER_DIGITS_DOUBLE_MINUTES,
    LS_TIMER_DIGITS_SINGLE_HOURS,
    LS_TIMER_DIGITS_DOUBLE_HOURS
} LSTimerDigitsFormat;

extern const LSComponentOption ls_timer_digits_format_options[];
extern const LSComponentOption ls_timer_timing_method_options[];
extern const LSComponentOption ls_timer_accuracy_options[];

ls_time_method ls_component_timer_method(LSComponent* component, ls_time_method default_method);

void ls_timer_set_style(GtkWidget* widget, ls_timer_status status);

void ls_timer_string(char* string, char* fraction, long long time, LSTimerDigitsFormat digits_format, LSTimeAccuracy accuracy);
