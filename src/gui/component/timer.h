/** \file timer.h
 *
 * Shared display helpers for the simple and detailed timers.
 */
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

/**
 * Gets the timer's configured timing method.
 *
 * @param component The timer component.
 * @param default_method Fallback for Default or unrecognized settings.
 * @return The selected timing method.
 */
ls_time_method ls_component_timer_method(LSComponent* component, ls_time_method default_method);

/**
 * Updates the timer's state classes without changing other CSS classes.
 *
 * @param widget The timer container.
 * @param status The current timer state.
 */
void ls_timer_set_style(GtkWidget* widget, ls_timer_status status);

/**
 * Formats microseconds into whole-time and fractional strings.
 *
 * @param string Whole-time output buffer, at least 32 bytes.
 * @param fraction Fraction output buffer including the decimal point, at least 8 bytes.
 * @param time Time in microseconds, or LLONG_MAX for an unavailable time.
 * @param digits_format Time fields and leading-zero format.
 * @param accuracy Fractional precision, truncated rather than rounded.
 */
void ls_timer_string(char* string, char* fraction, long long time, LSTimerDigitsFormat digits_format, LSTimeAccuracy accuracy);

/**
 * Updates timer text and scales long times.
 *
 * @param seconds The whole-time label.
 * @param fraction The fractional label.
 * @param text Whole-time text.
 * @param fraction_text Fractional text, including the decimal point.
 */
void ls_timer_set_text(GtkWidget* seconds, GtkWidget* fraction, const char* text, const char* fraction_text);

/**
 * Creates a timer row using GTK's box layout.
 *
 * @return The timer row, with gradients disabled.
 */
GtkWidget* ls_timer_box_new(void);

/**
 * Enables or disables gradient colors on a timer row.
 *
 * @param box A row created by ls_timer_box_new().
 * @param enabled Whether to use gradient colors.
 */
void ls_timer_set_gradient(GtkWidget* box, bool enabled);
