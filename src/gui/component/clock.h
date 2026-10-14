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

void ls_timer_string(char* string, char* fraction, long long time, LSTimerDigitsFormat digits_format, LSTimeAccuracy accuracy);
