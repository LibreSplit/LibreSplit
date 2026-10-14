/** \file splits.c
 *
 * Implementation of the splits component.
 */
#include "clock.h"
#include <gtk/gtk.h>
#include <limits.h>

#define SCROLL_TOLERANCE 0.5

typedef enum LSSplitsColumnType {
    LS_SPLITS_COLUMN_DELTA,
    LS_SPLITS_COLUMN_SPLIT_TIME,
    LS_SPLITS_COLUMN_SEGMENT_TIME
} LSSplitsColumnType;

typedef struct LSSplitsColumn {
    LSSplitsColumnType type;
    LSComponentComparisonMethod comparison_method;
    LSTimerDigitsFormat digits_format;
    GtkWidget** labels;
} LSSplitsColumn;

/**
 * @brief The component containing all the splits for the game.
 */
typedef struct LSSplits {
    LSComponent base; /*!< The base struct that is extended */
    unsigned int split_count; /*!< The number of splits */
    GtkWidget* container; /*!< The container for the splits */
    /*! The GTKBox containing all the split rows (except the last one, most of the time.)
     * The last split is added to this container when the scrollbox is scrolled all the way down. */
    GtkWidget* splits;
    GtkWidget* split_last; /*!< The last split container (used in case the split list is longer than the available space) */
    GtkAdjustment* split_adjust;
    GtkWidget* split_scroller; /*!< The scrollable window containing the split rows */
    GtkWidget* split_viewport;
    GtkWidget** split_rows;
    GtkWidget** split_titles;
    GtkWidget** split_icons;
    LSSplitsColumn* columns;
    size_t column_count;
    GtkCssProvider* icons_css_provider;
    gulong scroll_changed_handler;
} LSSplits;
extern LSComponentOps ls_splits_operations;

static const LSComponentOption time_accuracy_options[] = {
    { .key = LS_TIME_ACCURACY_DEFAULT, .label = "Default" },
    { .key = LS_TIME_ACCURACY_SECONDS, .label = "Seconds" },
    { .key = LS_TIME_ACCURACY_TENTHS, .label = "Tenths" },
    { .key = LS_TIME_ACCURACY_HUNDREDTHS, .label = "Hundredths" },
    { .key = LS_TIME_ACCURACY_MILLISECONDS, .label = "Milliseconds" },
    { 0 }
};

static const LSComponentOption column_type_options[] = {
    { .key = LS_SPLITS_COLUMN_DELTA, .label = "Delta" },
    { .key = LS_SPLITS_COLUMN_SPLIT_TIME, .label = "Split Time" },
    { .key = LS_SPLITS_COLUMN_SEGMENT_TIME, .label = "Segment Time" },
    { 0 }
};

static const LSComponentOption comparison_method_options[] = {
    { .key = LS_COMPONENT_COMPARISON_DEFAULT, .label = "Default" },
    { .key = LS_COMPONENT_COMPARISON_REAL_TIME, .label = "Real Time" },
    { .key = LS_COMPONENT_COMPARISON_GAME_TIME, .label = "Game Time" },
    { 0 }
};

static const LSComponentSetting column_settings[] = {
    { .key = "type", .label = "Column Type", .options = column_type_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_SPLITS_COLUMN_DELTA } },
    { .key = "comparison_method", .label = "Comparison Method", .options = comparison_method_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_COMPONENT_COMPARISON_DEFAULT } },
    { .key = "digits_format", .label = "Digits Format", .options = ls_timer_digits_format_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIMER_DIGITS_SINGLE_SECONDS } },
    { NULL }
};

static const LSComponentSetting split_time_column[] = {
    { .key = "type", .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_SPLITS_COLUMN_SPLIT_TIME } },
    { NULL }
};

static const LSComponentSetting* const default_columns[] = {
    column_settings,
    split_time_column
};

static const LSComponentSetting splits_settings[] = {
    { .key = "always_show_last_split", .label = "Always Show Last Split", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "split_time_accuracy", .label = "Split Time Accuracy", .options = time_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { .key = "segment_time_accuracy", .label = "Segment Time Accuracy", .options = time_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { .key = "delta_time_accuracy", .label = "Delta Time Accuracy", .options = time_accuracy_options, .val = { .type = LS_COMPONENT_SETTING_OPTIONS, .int_val = LS_TIME_ACCURACY_DEFAULT } },
    { .key = "columns", .label = "Columns", .val = { .type = LS_COMPONENT_SETTING_ARRAY, .array_val = { .values = default_columns, .count = G_N_ELEMENTS(default_columns) } } },
    { NULL }
};

static void splits_time_string(char* string, long long time, int accuracy, bool delta, LSTimerDigitsFormat digits_format)
{
    char fraction[8];
    if (delta && time >= 0 && time != LLONG_MAX) {
        *string++ = '+';
    }

    ls_timer_string(string, fraction, time, digits_format, accuracy);
    if (!delta || (time > -60000000LL && time < 60000000LL)) {
        strcat(string, fraction);
    }
}

static ls_time_method ls_splits_column_method(const LSSplitsColumn* column, ls_time_method default_method)
{
    switch (column->comparison_method) {
        case LS_COMPONENT_COMPARISON_DEFAULT:
            return default_method;
        case LS_COMPONENT_COMPARISON_REAL_TIME:
            return LS_REAL_TIME;
        case LS_COMPONENT_COMPARISON_GAME_TIME:
            return LS_GAME_TIME;
    }

    return default_method;
}

static long long ls_splits_segment_time(const ls_time* split_times, const ls_time* segment_times, unsigned int index, ls_time_method method)
{
    long long current = ls_time_get_by_method(split_times[index], method);
    if (!is_time_valid(current)) {
        return LLONG_MAX;
    }

    if (!index || is_time_valid(ls_time_get_by_method(split_times[index - 1], method))) {
        return ls_time_get_by_method(segment_times[index], method);
    }

    // After skipped splits, show the combined duration since the last recorded split.
    for (unsigned int i = index - 1; i > 0; --i) {
        long long previous = ls_time_get_by_method(split_times[i - 1], method);
        if (is_time_valid(previous)) {
            return current - previous;
        }
    }

    return current;
}

void free_all(LSSplits* self_)
{
    LSSplits* self = (LSSplits*)self_;

    free(self->split_rows);
    free(self->split_titles);
    free(self->split_icons);
    if (self->columns) {
        for (size_t i = 0; i < self->column_count; ++i) {
            free(self->columns[i].labels);
        }

        free(self->columns);
    }

    self->split_rows = NULL;
    self->split_titles = NULL;
    self->split_icons = NULL;
    self->columns = NULL;
    self->column_count = 0;
    self->split_count = 0;
}

/**
 * Constructor
 */
LSComponent* ls_component_splits_new(void)
{
    LSSplits* self;
    self = calloc(1, sizeof(LSSplits));
    if (!self) {
        return NULL;
    }

    self->base.ops = &ls_splits_operations;

    self->split_adjust = gtk_adjustment_new(0., 0., 0., 0., 0., 0.);

    self->split_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(self->split_scroller), self->split_adjust);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->split_scroller), GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
    gtk_widget_set_vexpand(self->split_scroller, TRUE);
    gtk_widget_set_hexpand(self->split_scroller, TRUE);

    self->split_viewport = gtk_viewport_new(NULL, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(self->split_scroller), self->split_viewport);

    self->splits = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->splits, "splits");
    gtk_widget_set_hexpand(self->splits, TRUE);
    gtk_viewport_set_child(GTK_VIEWPORT(self->split_viewport), self->splits);

    self->icons_css_provider = NULL;
    self->scroll_changed_handler = 0;

    self->split_last = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->split_last, "split-last");
    gtk_widget_set_hexpand(self->split_last, TRUE);

    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(self->container), self->split_scroller);
    gtk_box_append(GTK_BOX(self->container), self->split_last);
    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void splits_delete(LSComponent* self)
{
    LSSplits* splits = (LSSplits*)self;
    g_clear_signal_handler(&splits->scroll_changed_handler, splits->split_adjust);
    free_all(splits);
    if (splits->icons_css_provider) {
        gtk_style_context_remove_provider_for_display(
            gtk_widget_get_display(splits->container),
            GTK_STYLE_PROVIDER(splits->icons_css_provider));
        g_clear_object(&splits->icons_css_provider);
    }
    free(self);
}

/**
 * Returns the Splits container GTK widget.
 *
 * @param self The splits component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* splits_widget(LSComponent* self)
{
    return ((LSSplits*)self)->container;
}

static void scroll_to_bottom(GtkAdjustment* adjustment, gpointer data)
{
    LSSplits* self = data;
    double lower = gtk_adjustment_get_lower(adjustment);
    double upper = gtk_adjustment_get_upper(adjustment);
    double page_size = gtk_adjustment_get_page_size(adjustment);

    g_clear_signal_handler(&self->scroll_changed_handler, adjustment);
    gtk_adjustment_set_value(adjustment, MAX(lower, upper - page_size));
}

static void splits_trailer(LSComponent* self_)
{
    LSSplits* self = (LSSplits*)self_;
    bool always_show_last_split = ls_component_settings_get(self_, "always_show_last_split")->bool_val;
    int last = self->split_count - 1;
    if (!always_show_last_split) {
        g_clear_signal_handler(&self->scroll_changed_handler, self->split_adjust);
        if (gtk_widget_get_parent(self->split_rows[last]) == self->split_last) {
            g_object_ref(self->split_rows[last]);
            gtk_box_remove(GTK_BOX(self->split_last), self->split_rows[last]);
            gtk_box_append(GTK_BOX(self->splits), self->split_rows[last]);
            g_object_unref(self->split_rows[last]);
        }

        gtk_widget_set_visible(self->split_last, FALSE);
        return;
    }

    double curr_scroll = gtk_adjustment_get_value(self->split_adjust);
    double lower = gtk_adjustment_get_lower(self->split_adjust);
    double upper = gtk_adjustment_get_upper(self->split_adjust);
    double page_size = gtk_adjustment_get_page_size(self->split_adjust);
    double scroll_end = MAX(lower, upper - page_size);
    g_object_ref(self->split_rows[last]);
    if (gtk_widget_get_parent(self->split_rows[last]) == self->splits) {
        if (curr_scroll < scroll_end - SCROLL_TOLERANCE) {
            // move last split to split_last
            gtk_box_remove(GTK_BOX(self->splits),
                self->split_rows[last]);
            gtk_box_append(GTK_BOX(self->split_last),
                self->split_rows[last]);
            gtk_widget_set_visible(self->split_last, TRUE);
        }
    } else {
        if (curr_scroll >= scroll_end - SCROLL_TOLERANCE) {
            // move last split to split box
            g_clear_signal_handler(&self->scroll_changed_handler,
                self->split_adjust);
            self->scroll_changed_handler = g_signal_connect(
                self->split_adjust,
                "changed",
                G_CALLBACK(scroll_to_bottom),
                self);
            gtk_box_remove(GTK_BOX(self->split_last),
                self->split_rows[last]);
            gtk_box_append(GTK_BOX(self->splits),
                self->split_rows[last]);
            gtk_widget_set_visible(self->split_last, FALSE);
        }
    }

    g_object_unref(self->split_rows[last]);
}

/**
 * Function to execute when ls_app_window_show_game is executed.
 *
 * @param self_ The splits component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void splits_show_game(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    LSSplits* self = (LSSplits*)self_;
    char str[256];

    int split_time_accuracy = ls_component_settings_get(self_, "split_time_accuracy")->int_val;
    int segment_time_accuracy = ls_component_settings_get(self_, "segment_time_accuracy")->int_val;
    self->split_count = game->split_count;

    self->split_rows = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_rows) {
        // nothing has been allocated but call free_all
        // for consistency and to set split_count back to 0
        free_all(self);
        return;
    }

    self->split_titles = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_titles) {
        free_all(self);
        return;
    }

    self->split_icons = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_icons) {
        free_all(self);
        return;
    }

    self->column_count = ls_component_settings_get(self_, "columns")->array_val.count;
    self->columns = calloc(self->column_count, sizeof(LSSplitsColumn));
    if (self->column_count && !self->columns) {
        free_all(self);
        return;
    }

    for (size_t i = 0; i < self->column_count; ++i) {
        self->columns[i].type = ls_component_settings_array_get(self_, "columns", i, "type")->int_val;
        self->columns[i].comparison_method = ls_component_settings_array_get(self_, "columns", i, "comparison_method")->int_val;
        self->columns[i].digits_format = ls_component_settings_array_get(self_, "columns", i, "digits_format")->int_val;
        self->columns[i].labels = calloc(self->split_count, sizeof(GtkWidget*));
        if (self->split_count && !self->columns[i].labels) {
            free_all(self);
            return;
        }
    }

    GString* icons_css_src = g_string_new(".split-icon { background-repeat: no-repeat; background-position: center; min-width: 20px; min-height: 20px; background-size: 20px; margin-right: 4px; }");

    for (unsigned int i = 0; i < self->split_count; ++i) {
        self->split_rows[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        add_class(self->split_rows[i], "split");
        gtk_widget_set_hexpand(self->split_rows[i], TRUE);
        gtk_box_append(GTK_BOX(self->splits),
            self->split_rows[i]);

        self->split_titles[i] = gtk_label_new(game->split_titles[i]);
        add_class(self->split_titles[i], "split-title");
        gtk_widget_set_halign(self->split_titles[i], GTK_ALIGN_START);
        gtk_widget_set_hexpand(self->split_titles[i], TRUE);

        gchar* split_title_class = NULL;
        if (game->split_titles[i] && game->split_titles[i][0] != '\0') {
            split_title_class = g_strdup_printf("split-title-%s", game->split_titles[i]);
            gchar* c = split_title_class + strlen("split-title-");

            do {
                *c = g_ascii_isalnum(*c) ? g_ascii_tolower(*c) : '-';
            } while (*++c != '\0');

            add_class(self->split_rows[i], split_title_class);
        }

        if (game->contains_icons) {
            if (game->split_icon_paths[i] && split_title_class) {
                char* icon = icon_uri(game->path, game->split_icon_paths[i]);
                if (icon) {
                    g_string_append_printf(icons_css_src, ".%s .split-icon { background-image: url(", split_title_class);
                    append_quoted_uri(icons_css_src, icon);
                    g_string_append(icons_css_src, "); }");
                    g_free(icon);
                }
            }
            self->split_icons[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            add_class(self->split_icons[i], "split-icon");
            // set size but allow to dinamically change it from css with min-width and min-height
            gtk_widget_set_size_request(self->split_icons[i], 20, 20);
            gtk_box_append(GTK_BOX(self->split_rows[i]), self->split_icons[i]);
        }

        g_free(split_title_class);
        gtk_box_append(GTK_BOX(self->split_rows[i]), self->split_titles[i]);

        for (size_t column = 0; column < self->column_count; ++column) {
            GtkWidget* label = gtk_label_new(NULL);
            ls_time_method comparison_method = ls_splits_column_method(&self->columns[column], game->comparison_method);
            self->columns[column].labels[i] = label;
            if (self->columns[column].type == LS_SPLITS_COLUMN_DELTA) {
                add_class(label, "split-delta");
                gtk_widget_set_size_request(label, 1, -1);
            } else if (self->columns[column].type == LS_SPLITS_COLUMN_SPLIT_TIME) {
                add_class(label, "split-time");
                gtk_widget_set_halign(label, GTK_ALIGN_END);
                if (ls_time_get_by_method(game->split_times[i], comparison_method)) {
                    splits_time_string(str, ls_time_get_by_method(game->split_times[i], comparison_method), split_time_accuracy, false, self->columns[column].digits_format);
                    gtk_label_set_text(GTK_LABEL(label), str);
                }
            } else if (self->columns[column].type == LS_SPLITS_COLUMN_SEGMENT_TIME) {
                add_class(label, "split-time");
                add_class(label, "split-segment-time");
                gtk_widget_set_halign(label, GTK_ALIGN_END);
                long long time = ls_splits_segment_time(game->split_times, game->segment_times, i, comparison_method);
                if (time != LLONG_MAX) {
                    splits_time_string(str, time, segment_time_accuracy, false, self->columns[column].digits_format);
                    gtk_label_set_text(GTK_LABEL(label), str);
                }
            }

            gtk_box_append(GTK_BOX(self->split_rows[i]), label);
        }
    }

    if (self->icons_css_provider) {
        // remove old css provider
        gtk_style_context_remove_provider_for_display(
            gtk_widget_get_display(self->container),
            GTK_STYLE_PROVIDER(self->icons_css_provider));
        g_object_unref(self->icons_css_provider);
        self->icons_css_provider = NULL;
    }

    if (icons_css_src && icons_css_src->len > 0) {
        self->icons_css_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(
            self->icons_css_provider,
            icons_css_src->str);
        // add new css provider
        gtk_style_context_add_provider_for_display(
            gtk_widget_get_display(self->container),
            GTK_STYLE_PROVIDER(self->icons_css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_string_free(icons_css_src, TRUE);
    }

    gtk_widget_set_visible(self->splits, TRUE);
    if (self->split_count) {
        splits_trailer(self_);
    }
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The splits component itself.
 */
static void splits_clear_game(LSComponent* self_)
{
    LSSplits* self = (LSSplits*)self_;
    g_clear_signal_handler(&self->scroll_changed_handler, self->split_adjust);
    gtk_widget_set_visible(self->splits, FALSE);
    gtk_widget_set_visible(self->split_last, FALSE);
    for (int i = self->split_count - 1; i >= 0; --i) {
        GtkWidget* parent = gtk_widget_get_parent(self->split_rows[i]);
        if (parent == self->splits) {
            gtk_box_remove(GTK_BOX(self->splits), self->split_rows[i]);
        } else if (parent == self->split_last) {
            gtk_box_remove(GTK_BOX(self->split_last), self->split_rows[i]);
        }
    }

    gtk_adjustment_set_value(self->split_adjust, 0);
    free_all(self);
    self->split_count = 0;
}

#define SHOW_DELTA_THRESHOLD (-30 * 1000000LL)
/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The splits component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void splits_draw(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    LSSplits* self = (LSSplits*)self_;
    int split_time_accuracy = ls_component_settings_get(self_, "split_time_accuracy")->int_val;
    int segment_time_accuracy = ls_component_settings_get(self_, "segment_time_accuracy")->int_val;
    int delta_time_accuracy = ls_component_settings_get(self_, "delta_time_accuracy")->int_val;
    char str[256];

    for (unsigned int i = 0; i < self->split_count; ++i) {
        if (i == timer->curr_split
            && timer->started) {
            add_class(self->split_rows[i], "current-split");
        } else {
            remove_class(self->split_rows[i], "current-split");
        }

        for (size_t column = 0; column < self->column_count; ++column) {
            GtkWidget* label = self->columns[column].labels[i];
            ls_time_method comparison_method = ls_splits_column_method(&self->columns[column], game->comparison_method);
            if (self->columns[column].type == LS_SPLITS_COLUMN_SPLIT_TIME) {
                remove_class(label, "time");
                remove_class(label, "done");
                // Set split_times label to -
                gtk_label_set_text(GTK_LABEL(label), "-");

                if (i < timer->curr_split) {
                    add_class(label, "done");
                    if (ls_time_get_by_method(timer->split_times[i], comparison_method)) {
                        add_class(label, "time");
                        splits_time_string(str, ls_time_get_by_method(timer->split_times[i], comparison_method), split_time_accuracy, false, self->columns[column].digits_format);
                        gtk_label_set_text(GTK_LABEL(label), str);
                    }
                } else if (ls_time_get_by_method(game->split_times[i], comparison_method)) {
                    add_class(label, "time");
                    splits_time_string(str, ls_time_get_by_method(game->split_times[i], comparison_method), split_time_accuracy, false, self->columns[column].digits_format);
                    gtk_label_set_text(GTK_LABEL(label), str);
                }
            } else if (self->columns[column].type == LS_SPLITS_COLUMN_DELTA) {
                remove_class(label, "best-split");
                remove_class(label, "best-segment");
                remove_class(label, "behind");
                remove_class(label, "losing");
                remove_class(label, "delta");
                gtk_label_set_text(GTK_LABEL(label), "");
                if (i < timer->curr_split
                    || ls_time_get_by_method(timer->split_deltas[i], comparison_method) >= SHOW_DELTA_THRESHOLD) {
                    if (timer->split_info[i] & LS_INFO_BEST_SPLIT) {
                        add_class(label, "best-split");
                    }
                    if (timer->split_info[i] & LS_INFO_BEST_SEGMENT) {
                        add_class(label, "best-segment");
                    }
                    if (timer->split_info[i] & LS_INFO_BEHIND_TIME) {
                        add_class(label, "behind");
                        if (timer->split_info[i]
                            & LS_INFO_LOSING_TIME) {
                            add_class(label, "losing");
                        }
                    } else {
                        remove_class(label, "behind");
                        if (timer->split_info[i]
                            & LS_INFO_LOSING_TIME) {
                            add_class(label, "losing");
                        }
                    }
                    if (ls_time_get_by_method(timer->split_deltas[i], comparison_method)) {
                        add_class(label, "delta");
                        splits_time_string(str, ls_time_get_by_method(timer->split_deltas[i], comparison_method), delta_time_accuracy, true, self->columns[column].digits_format);
                        gtk_label_set_text(GTK_LABEL(label), str);
                    }
                }
            } else if (self->columns[column].type == LS_SPLITS_COLUMN_SEGMENT_TIME) {
                remove_class(label, "time");
                remove_class(label, "done");
                gtk_label_set_text(GTK_LABEL(label), "-");

                if (i < timer->curr_split) {
                    add_class(label, "done");
                    long long time = ls_splits_segment_time(timer->split_times, timer->segment_times, i, comparison_method);
                    if (time != LLONG_MAX) {
                        add_class(label, "time");
                        splits_time_string(str, time, segment_time_accuracy, false, self->columns[column].digits_format);
                        gtk_label_set_text(GTK_LABEL(label), str);
                    }
                } else {
                    long long time = ls_splits_segment_time(game->split_times, game->segment_times, i, comparison_method);
                    if (time != LLONG_MAX) {
                        add_class(label, "time");
                        splits_time_string(str, time, segment_time_accuracy, false, self->columns[column].digits_format);
                        gtk_label_set_text(GTK_LABEL(label), str);
                    }
                }
            }
        }
    }

    // keep each column's sizes in sync
    for (size_t column = 0; column < self->column_count; ++column) {
        int column_width = 0;
        for (unsigned int i = 0; i < self->split_count; ++i) {
            int width = gtk_widget_get_width(self->columns[column].labels[i]);
            if (width > column_width) {
                column_width = width;
            }
        }

        if (!column_width) {
            continue;
        }

        for (unsigned int i = 0; i < self->split_count; ++i) {
            GtkWidget* label = self->columns[column].labels[i];
            int margin = column ? 8 * 2 : 0;
            if (self->columns[column].type == LS_SPLITS_COLUMN_DELTA) {
                gtk_widget_set_size_request(label, column_width, -1);
            } else {
                margin += column_width - gtk_widget_get_width(label);
            }
            gtk_widget_set_margin_start(label, margin);
        }
    }

    if (self->split_count) {
        splits_trailer(self_);
    }
}

/**
 * Scrolls to the current split if it's not visible.
 *
 * @param self_ The splits component itself.
 * @param timer The timer instance.
 */
static void splits_scroll_to_split(LSComponent* self_, const ls_timer* timer)
{
    LSSplits* self = (LSSplits*)self_;
    int split_h;
    int scroller_h;
    double curr_scroll;
    double min_scroll, max_scroll;
    const graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0);
    graphene_point_t split_position;

    if (timer->game->split_count == 0)
        return;

    unsigned int prev = timer->curr_split ? timer->curr_split - 1 : 0;
    unsigned int curr = timer->curr_split;
    unsigned int next = timer->curr_split + 1;
    if (prev < 0) {
        prev = 0;
    }
    if (curr >= self->split_count) {
        curr = self->split_count - 1;
    }
    if (next >= self->split_count) {
        next = self->split_count - 1;
    }
    curr_scroll = gtk_adjustment_get_value(self->split_adjust);
    if (!gtk_widget_compute_point(
            self->split_titles[prev],
            self->split_viewport,
            &origin, &split_position)) {
        return;
    }
    scroller_h = gtk_widget_get_height(self->split_scroller);
    split_h = gtk_widget_get_height(self->split_titles[prev]);
    if (curr != next && curr != prev) {
        split_h += gtk_widget_get_height(self->split_titles[curr]);
    }
    if (next != prev) {
        int h = gtk_widget_get_height(self->split_titles[next]);
        if (split_h + h < scroller_h) {
            split_h += h;
        }
    }
    min_scroll = split_position.y + curr_scroll - scroller_h + split_h;
    max_scroll = split_position.y + curr_scroll;
    if (curr_scroll > max_scroll) {
        gtk_adjustment_set_value(self->split_adjust, max_scroll);
    } else if (curr_scroll < min_scroll) {
        gtk_adjustment_set_value(self->split_adjust, min_scroll);
    }
}

void splits_start_split(LSComponent* self, const ls_timer* timer)
{
    splits_scroll_to_split(self, timer);
}

void splits_skip(LSComponent* self, const ls_timer* timer)
{
    splits_scroll_to_split(self, timer);
}

void splits_unsplit(LSComponent* self, const ls_timer* timer)
{
    splits_scroll_to_split(self, timer);
}

LSComponentOps ls_splits_operations = {
    .settings = splits_settings,
    .delete = splits_delete,
    .widget = splits_widget,
    .show_game = splits_show_game,
    .clear_game = splits_clear_game,
    .draw = splits_draw,
    .start_split = splits_start_split,
    .skip = splits_skip,
    .unsplit = splits_unsplit
};
