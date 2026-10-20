/** \file run-predict.c
 *
 * Run Predict component.
 */
#include "components.h"

typedef struct LSRunPrediction {
    LSComponent base;
    GtkWidget* container;
    GtkWidget* time;
    ls_time* comparisons;
} LSRunPrediction;
extern LSComponentOps ls_run_predict_operations;

typedef enum LSRunPredictType {
    LS_BEST_POSSIBLE_TIME,
    LS_CURRENT_PACE
} LSRunPredictType;

static const char* const RUN_PREDICT_LABELS[] = {
    [LS_BEST_POSSIBLE_TIME] = "Best Possible Time",
    [LS_CURRENT_PACE] = "Current Pace"
};

static const LSComponentOption type_options[] = {
    { .key = LS_BEST_POSSIBLE_TIME, .label = RUN_PREDICT_LABELS[LS_BEST_POSSIBLE_TIME] },
    { .key = LS_CURRENT_PACE, .label = RUN_PREDICT_LABELS[LS_CURRENT_PACE] },
    { 0 }
};

static const LSComponentSetting run_predict_settings[] = {
    { .key = "type", .label = "Predict Type", .options = type_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_CURRENT_PACE } },
    { NULL }
};

static bool has_split_time(ls_time time, ls_time_method method)
{
    long long value = ls_time_get_by_method(time, method);
    return is_time_valid(value) || (method == LS_GAME_TIME && value == 0 && is_time_valid(time.real_time));
}

/** Returns the projected finish time, or LLONG_MAX when unavailable. */
static long long run_predict_time(const LSRunPrediction* self, const ls_timer* timer, ls_time_method method)
{
    const ls_game* game = timer->game;
    if (!game->split_count || !self->comparisons) {
        return LLONG_MAX;
    }

    if (timer->curr_split == game->split_count) {
        ls_time final = timer->split_times[game->split_count - 1];
        return has_split_time(final, method) ? ls_time_get_by_method(final, method) : LLONG_MAX;
    }

    LSRunPredictType type = ls_component_settings_get((LSComponent*)self, "type")->int_val;
    long long total = ls_time_get_by_method(self->comparisons[game->split_count - 1], method);
    if (total == LLONG_MAX) {
        return LLONG_MAX;
    }

    if (!timer->started) {
        switch (type) {
            case LS_BEST_POSSIBLE_TIME:
                return total;

            case LS_CURRENT_PACE:
                return LLONG_MAX;
        }

        return LLONG_MAX;
    }

    long long delta = 0;
    for (unsigned int i = timer->curr_split; i > 0; --i) {
        long long comparison = ls_time_get_by_method(self->comparisons[i - 1], method);
        if (comparison != LLONG_MAX && has_split_time(timer->split_times[i - 1], method)) {
            delta = ls_time_get_by_method(timer->split_times[i - 1], method) - comparison;
            break;
        }
    }

    long long current_comparison = ls_time_get_by_method(self->comparisons[timer->curr_split], method);
    long long current = ls_time_get_by_method(ls_timer_get_time(timer, false), method);
    // Keep the last completed delta until the live delta is worse.
    if (current >= 0 && current < LLONG_MAX && current_comparison != LLONG_MAX) {
        delta = MAX(delta, current - current_comparison);
    }

    return delta > LLONG_MAX - total ? LLONG_MAX : total + delta;
}

static void run_predict_delete(LSComponent* self)
{
    g_free(((LSRunPrediction*)self)->comparisons);
    free(self);
}

static GtkWidget* run_predict_widget(LSComponent* self)
{
    return ((LSRunPrediction*)self)->container;
}

static void run_predict_draw(LSComponent* component, const ls_game* game, const ls_timer* timer)
{
    LSRunPrediction* self = (LSRunPrediction*)component;
    long long time = run_predict_time(self, timer, game->comparison_method);
    char str[MAX_TIMESTAMP_LENGTH];
    ls_time_string(str, time);
    gtk_label_set_text(GTK_LABEL(self->time), str);
    if (time == LLONG_MAX) {
        remove_class(self->time, "time");
    } else {
        add_class(self->time, "time");
    }
}

/** Builds the comparison once, preserving it through splits and undo. */
static void run_predict_show_game(LSComponent* component, const ls_game* game, const ls_timer* timer)
{
    LSRunPredictType type = ls_component_settings_get(component, "type")->int_val;
    LSRunPrediction* self = (LSRunPrediction*)component;
    g_free(self->comparisons);
    self->comparisons = g_new(ls_time, game->split_count);
    for (ls_time_method method = LS_REAL_TIME; method <= LS_GAME_TIME; ++method) {
        long long best = 0;
        long long previous_pb = 0;
        long long previous_pb_best = 0;
        for (unsigned int i = 0; i < game->split_count; ++i) {
            long long pb = ls_time_get_by_method(game->split_times[i], method);
            long long comparison = is_time_valid(pb) ? pb : LLONG_MAX;
            if (type == LS_BEST_POSSIBLE_TIME) {
                long long segment = ls_time_get_by_method(game->best_segments[i], method);
                best = is_time_valid(segment) && segment < LLONG_MAX - best ? best + segment : LLONG_MAX;
                if (is_time_valid(pb)) {
                    segment = pb - previous_pb;
                    if (segment >= 0 && segment < LLONG_MAX - previous_pb_best) {
                        best = MIN(best, previous_pb_best + segment);
                    }

                    previous_pb = pb;
                    previous_pb_best = best;
                }

                comparison = best;
            }

            if (method == LS_REAL_TIME) {
                self->comparisons[i].real_time = comparison;
            } else {
                self->comparisons[i].game_time = comparison;
            }
        }
    }

    run_predict_draw(component, game, timer);
}

static void run_predict_clear_game(LSComponent* component)
{
    LSRunPrediction* self = (LSRunPrediction*)component;
    g_clear_pointer(&self->comparisons, g_free);
    gtk_label_set_text(GTK_LABEL(self->time), "");
    remove_class(self->time, "time");
}

LSComponent* ls_component_run_predict_new(json_t* settings)
{
    LSRunPrediction* self = calloc(1, sizeof(*self));
    if (!self) {
        return NULL;
    }

    self->base.ops = &ls_run_predict_operations;
    ls_component_settings_load((LSComponent*)self, settings);
    LSRunPredictType type = ls_component_settings_get((LSComponent*)self, "type")->int_val;

    self->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->container, "statistics");

    GtkWidget* label = gtk_label_new(RUN_PREDICT_LABELS[type]);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(self->container), label);

    self->time = gtk_label_new(NULL);
    gtk_widget_set_halign(self->time, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(self->container), self->time);
    add_class(self->container, "run-predict-container");
    add_class(label, "run-predict-label");
    add_class(self->time, "run-predict-time");

    switch (type) {
        case LS_BEST_POSSIBLE_TIME:
            add_class(self->container, "best-possible-time-container");
            add_class(label, "best-possible-time-label");
            add_class(self->time, "best-possible-time");
            break;

        case LS_CURRENT_PACE:
            add_class(self->container, "current-pace-container");
            add_class(label, "current-pace-label");
            add_class(self->time, "current-pace");
            break;
    }

    return (LSComponent*)self;
}

LSComponentOps ls_run_predict_operations = {
    .settings = run_predict_settings,
    .delete = run_predict_delete,
    .widget = run_predict_widget,
    .show_game = run_predict_show_game,
    .clear_game = run_predict_clear_game,
    .draw = run_predict_draw
};
