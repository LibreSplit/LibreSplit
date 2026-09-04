/** \file splits.c
 *
 * Implementation of the splits component.
 */
#include "components.h"
#include <gtk/gtk.h>
#include <limits.h>

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
    GtkWidget** split_deltas;
    GtkWidget** split_times;
    GtkCssProvider* icons_css_provider;
    gulong scroll_changed_handler;
} LSSplits;
extern LSComponentOps ls_splits_operations;

void free_all(LSSplits* self_)
{
    LSSplits* self = (LSSplits*)self_;
    if (self->split_rows) {
        free(self->split_rows);
    }
    if (self->split_titles) {
        free(self->split_titles);
    }
    if (self->split_icons) {
        free(self->split_icons);
    }
    if (self->split_deltas) {
        free(self->split_deltas);
    }
    if (self->split_times) {
        free(self->split_times);
    }
}

/**
 * Constructor
 */
LSComponent* ls_component_splits_new(void)
{
    LSSplits* self;

    self = malloc(sizeof(LSSplits));
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

#define SCROLL_TOLERANCE 0.5

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
    int last = self->split_count - 1;
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
static void splits_show_game(LSComponent* self_, const ls_game* game,
    const ls_timer* timer)
{
    LSSplits* self = (LSSplits*)self_;
    char str[256];
    self->split_count = game->split_count;

    self->split_rows = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_rows)
        return;

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

    self->split_deltas = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_deltas) {
        free_all(self);
        return;
    }

    self->split_times = calloc(self->split_count, sizeof(GtkWidget*));
    if (!self->split_times) {
        free_all(self);
        return;
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

        if (game->split_titles[i]
            && strlen(game->split_titles[i])) {
            char* c = &str[12];
            strcpy(str, "split-title-");
            strcpy(c, game->split_titles[i]);
            do {
                if (!isalnum(*c)) {
                    *c = '-';
                } else {
                    *c = tolower(*c);
                }
            } while (*++c != '\0');
            {
                add_class(self->split_rows[i], str);
            }
        }

        if (game->contains_icons) {
            if (game->split_icon_paths[i]) {
                // g_string_append_printf(icons_css_src, ".split:nth-child(%d) .split-icon { background-image: url('%s'); }", i+1, game->split_icon_paths[i]);
                g_string_append_printf(
                    icons_css_src,
                    ".%s .split-icon { background-image: url('%s'); }",
                    str, game->split_icon_paths[i]);
            }
            self->split_icons[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            add_class(self->split_icons[i], "split-icon");
            // set size but allow to dinamically change it from css with min-width and min-height
            gtk_widget_set_size_request(self->split_icons[i], 20, 20);
            gtk_box_append(GTK_BOX(self->split_rows[i]), self->split_icons[i]);
        }
        gtk_box_append(GTK_BOX(self->split_rows[i]), self->split_titles[i]);

        self->split_deltas[i] = gtk_label_new(NULL);
        add_class(self->split_deltas[i], "split-delta");
        gtk_widget_set_size_request(self->split_deltas[i], 1, -1);
        gtk_box_append(GTK_BOX(self->split_rows[i]),
            self->split_deltas[i]);

        self->split_times[i] = gtk_label_new(NULL);
        add_class(self->split_times[i], "split-time");
        gtk_widget_set_halign(self->split_times[i], GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(self->split_rows[i]),
            self->split_times[i]);

        if (ls_time_get_by_method(game->split_times[i], game->comparison_method)) {
            ls_split_string(str, ls_time_get_by_method(game->split_times[i], game->comparison_method), 0);
            gtk_label_set_text(GTK_LABEL(self->split_times[i]), str);
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

    if (icons_css_src->len > 0) {
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
    if (self->split_count)
        splits_trailer(self_);
}

/**
 * Function to execute when ls_app_window_clear_game is executed.
 *
 * @param self_ The splits component itself.
 */
static void splits_clear_game(LSComponent* self_)
{
    LSSplits* self = (LSSplits*)self_;
    int i;
    g_clear_signal_handler(&self->scroll_changed_handler, self->split_adjust);
    gtk_widget_set_visible(self->splits, FALSE);
    gtk_widget_set_visible(self->split_last, FALSE);
    for (i = self->split_count - 1; i >= 0; --i) {
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
    char str[256];
    for (unsigned int i = 0; i < self->split_count; ++i) {
        if (i == timer->curr_split
            && timer->started) {
            add_class(self->split_rows[i], "current-split");
        } else {
            remove_class(self->split_rows[i], "current-split");
        }

        remove_class(self->split_times[i], "time");
        remove_class(self->split_times[i], "done");

        // Set split_times label to -
        gtk_label_set_text(GTK_LABEL(self->split_times[i]), "-");

        if (i < timer->curr_split) {
            add_class(self->split_times[i], "done");
            if (ls_time_get_by_method(timer->split_times[i], game->comparison_method)) {
                add_class(self->split_times[i], "time");
                ls_split_string(str, ls_time_get_by_method(timer->split_times[i], game->comparison_method), 0);
                gtk_label_set_text(GTK_LABEL(self->split_times[i]), str);
            }
        } else if (ls_time_get_by_method(game->split_times[i], game->comparison_method)) {
            add_class(self->split_times[i], "time");
            ls_split_string(str, ls_time_get_by_method(game->split_times[i], game->comparison_method), 0);
            gtk_label_set_text(GTK_LABEL(self->split_times[i]), str);
        }

        remove_class(self->split_deltas[i], "best-split");
        remove_class(self->split_deltas[i], "best-segment");
        remove_class(self->split_deltas[i], "behind");
        remove_class(self->split_deltas[i], "losing");
        remove_class(self->split_deltas[i], "delta");
        gtk_label_set_text(GTK_LABEL(self->split_deltas[i]), "");
        if (i < timer->curr_split
            || ls_time_get_by_method(timer->split_deltas[i], game->comparison_method) >= SHOW_DELTA_THRESHOLD) {
            if (timer->split_info[i] & LS_INFO_BEST_SPLIT) {
                add_class(self->split_deltas[i], "best-split");
            }
            if (timer->split_info[i] & LS_INFO_BEST_SEGMENT) {
                add_class(self->split_deltas[i], "best-segment");
            }
            if (timer->split_info[i] & LS_INFO_BEHIND_TIME) {
                add_class(self->split_deltas[i], "behind");
                if (timer->split_info[i]
                    & LS_INFO_LOSING_TIME) {
                    add_class(self->split_deltas[i], "losing");
                }
            } else {
                remove_class(self->split_deltas[i], "behind");
                if (timer->split_info[i]
                    & LS_INFO_LOSING_TIME) {
                    add_class(self->split_deltas[i], "losing");
                }
            }
            if (ls_time_get_by_method(timer->split_deltas[i], game->comparison_method)) {
                add_class(self->split_deltas[i], "delta");
                ls_delta_string(str, ls_time_get_by_method(timer->split_deltas[i], game->comparison_method));
                gtk_label_set_text(GTK_LABEL(self->split_deltas[i]), str);
            }
        }
    }

    // keep split sizes in sync
    if (self->split_count) {
        int width;
        int time_width = 0, delta_width = 0;
        for (unsigned int i = 0; i < self->split_count; ++i) {
            width = gtk_widget_get_width(self->split_deltas[i]);
            if (width > delta_width) {
                delta_width = width;
            }
            width = gtk_widget_get_width(self->split_times[i]);
            if (width > time_width) {
                time_width = width;
            }
        }
        for (unsigned int i = 0; i < self->split_count; ++i) {
            if (delta_width) {
                gtk_widget_set_size_request(
                    self->split_deltas[i], delta_width, -1);
            }
            if (time_width) {
                width = gtk_widget_get_width(
                    self->split_times[i]);
                gtk_widget_set_margin_start(self->split_times[i],
                    /*WINDOW_PAD*/ 8 * 2 + (time_width - width));
            }
        }
    }

    if (self->split_count)
        splits_trailer(self_);
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
    .delete = splits_delete,
    .widget = splits_widget,
    .show_game = splits_show_game,
    .clear_game = splits_clear_game,
    .draw = splits_draw,
    .start_split = splits_start_split,
    .skip = splits_skip,
    .unsplit = splits_unsplit
};
