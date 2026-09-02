/** \file splits.c
 *
 * Implementation of the splits component.
 */
#include "components.h"
#include <gtk/gtk.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../../timer.h"

#define NO_SUBSPLIT_GROUP (-1)

/**
 * @brief A subsplit group: a range of consecutive splits grouped under a
 *        common header.
 */
typedef struct split_group {
    char* name; /*!< The group header name */
    unsigned int start_idx; /*!< Index of the first split in the group */
    unsigned int end_idx; /*!< Index of the split that closes the group */
} split_group;

/**
 * @brief Parsed subsplit group data, derived from split title prefixes.
 */
typedef struct SubsplitData {
    unsigned int group_count; /*!< The number of subsplit groups */
    int* split_group_index; /*!< For each split, its group index or NO_SUBSPLIT_GROUP */
    split_group* groups; /*!< Array of group_count subsplit groups */
} SubsplitData;

/**
 * @brief The GTK widgets that make up a group header row.
 */
typedef struct GroupHeaderWidgets {
    GtkWidget* row; /*!< The header row widget */
    GtkWidget* title; /*!< The group name label */
    GtkWidget* time; /*!< The group total time label */
    GtkWidget* delta; /*!< The group delta label */
} GroupHeaderWidgets;

/**
 * @brief Returns the display title for a split, stripping any subsplit
 *        group prefix ("-" or "{group_name}").
 *
 * @param title The raw split title.
 * @return The title without its prefix; the pointer returned points inside
 *         the original title string (or a static "" if empty).
 */
static const char* subsplit_display_title(const char* title)
{
    if (!title || !title[0])
        return "";
    if (title[0] == '-') {
        // skip the "-" and any leading spaces: points to the subsplit name
        const char* p = title + 1;
        while (*p == ' ')
            p++;
        return p;
    }
    if (title[0] == '{') {
        // same as above, but for group header: skip "{group_name}" and any
        // leading spaces, pointing to the split's own name
        const char* close = strchr(title, '}');
        if (close) {
            const char* p = close + 1;
            while (*p == ' ')
                p++;
            return p;
        }
    }
    return title;
}

/**
 * @brief Checks whether the current split is inside the given group.
 *
 * @param group The group to check.
 * @param timer The timer instance.
 * @return True if the current split is between the group's start and end.
 */
static bool group_is_active(const split_group* group, const ls_timer* timer)
{
    return timer->curr_split >= group->start_idx
        && timer->curr_split <= group->end_idx;
}

/**
 * @brief Parses split titles for subsplit group markers and builds the
 *        group data.
 *
 * Splits prefixed with "-" are subsplit items; a split prefixed with
 * "{group_name}" holds the group header (followed by the last split). The group spans the first subsplit item
 * after the previous group through to the last split.
 *
 * @param subsplit The SubsplitData to fill in.
 * @param game The game whose split titles are parsed.
 * @param split_count The number of splits in the game.
 * @return 0 on success, -1 on allocation failure.
 */
static int parse_subsplits(SubsplitData* subsplit, const struct ls_game* game,
    unsigned int split_count)
{
    subsplit->split_group_index = calloc(split_count, sizeof(int));
    if (!subsplit->split_group_index)
        return -1;

    for (unsigned int i = 0; i < split_count; ++i)
        subsplit->split_group_index[i] = NO_SUBSPLIT_GROUP;

    subsplit->group_count = 0;
    subsplit->groups = NULL;

    int group_start = -1;

    for (unsigned int i = 0; i < split_count; ++i) {
        const char* title = game->split_titles[i];
        if (!title || !title[0]) {
            group_start = -1;
            continue;
        }

        if (title[0] == '-') {
            if (group_start == -1)
                group_start = i;
        } else if (title[0] == '{') {
            const char* close = strchr(title, '}');
            if (close) {
                char* group_name = strndup(title + 1, close - title - 1);
                if (!group_name)
                    return -1;

                if (group_start == -1)
                    group_start = i;

                split_group* new_groups = realloc(subsplit->groups,
                    (subsplit->group_count + 1) * sizeof(split_group));
                if (!new_groups) {
                    free(group_name);
                    return -1;
                }
                subsplit->groups = new_groups;
                subsplit->groups[subsplit->group_count].name = group_name;
                subsplit->groups[subsplit->group_count].start_idx = group_start;
                subsplit->groups[subsplit->group_count].end_idx = i;
                for (unsigned int s = group_start; s <= i; ++s)
                    subsplit->split_group_index[s] = subsplit->group_count;
                ++subsplit->group_count;
                group_start = -1;
            } else {
                group_start = -1;
            }
        } else {
            group_start = -1;
        }
    }

    return 0;
}

/**
 * @brief The component containing all the splits for the game.
 */
typedef struct LSSplits {
    LSComponent base; /*!< The base struct that is extended */
    unsigned int split_count; /*!< The number of splits */
    GtkWidget* container; /*!< The container for the splits */
    SubsplitData subsplit_data; /*!< The data required to render subsplits for the run*/
    GtkWidget* splits;
    GtkWidget* split_last;
    GtkAdjustment* split_adjust;
    GtkWidget* split_scroller;
    GtkWidget* split_viewport;
    GtkWidget** split_rows;
    GtkWidget** split_titles;
    GtkWidget** split_icons;
    GtkWidget** split_deltas;
    GtkWidget** split_times;
    GroupHeaderWidgets* group_headers;
    GtkCssProvider* icons_css_provider;
} LSSplits;

extern LSComponentOps ls_splits_operations;

/**
 * @brief Frees all heap-allocated arrays owned by the splits component.
 *
 * @param self_ The splits component whose arrays should be freed.
 */
void free_all(LSSplits* self_)
{
    LSSplits* self = self_;
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
    if (self->group_headers) {
        free(self->group_headers);
        self->group_headers = NULL;
    }
    if (self->subsplit_data.split_group_index) {
        free(self->subsplit_data.split_group_index);
        self->subsplit_data.split_group_index = NULL;
    }
    if (self->subsplit_data.groups) {
        for (unsigned int i = 0; i < self->subsplit_data.group_count; ++i) {
            if (self->subsplit_data.groups[i].name) {
                free(self->subsplit_data.groups[i].name);
            }
        }
        free(self->subsplit_data.groups);
        self->subsplit_data.groups = NULL;
    }
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

    self->split_scroller = gtk_scrolled_window_new(NULL, self->split_adjust);
    gtk_widget_set_vexpand(self->split_scroller, TRUE);
    gtk_widget_set_hexpand(self->split_scroller, TRUE);
    gtk_widget_show(self->split_scroller);
    gtk_widget_add_events(self->split_scroller, GDK_SCROLL_MASK);
    gtk_widget_hide(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(self->split_scroller)));
    gtk_widget_hide(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(self->split_scroller)));

    self->split_viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(self->split_scroller),
        self->split_viewport);
    gtk_widget_show(self->split_viewport);

    self->splits = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->splits, "splits");
    gtk_widget_set_hexpand(self->splits, TRUE);
    gtk_container_add(GTK_CONTAINER(self->split_viewport), self->splits);
    gtk_widget_show(self->splits);

    self->icons_css_provider = NULL;

    self->split_last = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(self->split_last, "split-last");
    gtk_widget_set_hexpand(self->split_last, TRUE);
    gtk_widget_show(self->split_last);

    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(self->container), self->split_scroller);
    gtk_container_add(GTK_CONTAINER(self->container), self->split_last);
    gtk_widget_show(self->container);
    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void splits_delete(LSComponent* self)
{
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

static void splits_trailer(LSComponent* self_)
{
    LSSplits* self = (LSSplits*)self_;
    int height, split_h, last = self->split_count - 1;
    double curr_scroll = gtk_adjustment_get_value(self->split_adjust);
    double scroll_max = gtk_adjustment_get_upper(self->split_adjust);
    double page_size = gtk_adjustment_get_page_size(self->split_adjust);
    g_object_ref(self->split_rows[last]);
    split_h = gtk_widget_get_allocated_height(self->split_titles[last]);
    height = gtk_widget_get_allocated_height(self->splits);
    if (gtk_widget_get_parent(self->split_rows[last]) == self->splits) {
        if (curr_scroll + page_size < scroll_max) {
            // move last split to split_last
            gtk_container_remove(GTK_CONTAINER(self->splits),
                self->split_rows[last]);
            gtk_container_add(GTK_CONTAINER(self->split_last),
                self->split_rows[last]);
            gtk_widget_show(self->split_last);
        }
    } else {
        if (curr_scroll + page_size == scroll_max) {
            // move last split to split box
            gtk_container_remove(GTK_CONTAINER(self->split_last),
                self->split_rows[last]);
            gtk_container_add(GTK_CONTAINER(self->splits),
                self->split_rows[last]);
            gtk_adjustment_set_upper(self->split_adjust,
                scroll_max + height);
            gtk_adjustment_set_value(self->split_adjust,
                curr_scroll + split_h);
            gtk_widget_hide(self->split_last);
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

    // Parse subsplit prefixes and build groups
    if (parse_subsplits(&self->subsplit_data, game, self->split_count) != 0) {
        free_all(self);
        return;
    }

    GString* icons_css_src = g_string_new(".split-icon { background-repeat: no-repeat; background-position: center; min-width: 20px; min-height: 20px; background-size: 20px; margin-right: 4px; }");

    for (unsigned int i = 0; i < self->split_count; ++i) {
        const char* display_title = subsplit_display_title(game->split_titles[i]);

        self->split_rows[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        add_class(self->split_rows[i], "split");
        gtk_widget_set_hexpand(self->split_rows[i], TRUE);
        gtk_container_add(GTK_CONTAINER(self->splits),
            self->split_rows[i]);

        if (self->subsplit_data.split_group_index[i] >= 0) {
            add_class(self->split_rows[i], "subsplit");
            GtkWidget* indent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_size_request(indent, 24, -1);
            gtk_container_add(GTK_CONTAINER(self->split_rows[i]), indent);
        }

        self->split_titles[i] = gtk_label_new(display_title);
        if (self->subsplit_data.split_group_index[i] >= 0) {
            add_class(self->split_titles[i], "subsplit-title");
        }
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
            gtk_container_add(GTK_CONTAINER(self->split_rows[i]), self->split_icons[i]);
            gtk_widget_show(self->split_icons[i]);
        }
        gtk_container_add(GTK_CONTAINER(self->split_rows[i]), self->split_titles[i]);

        self->split_deltas[i] = gtk_label_new(NULL);
        add_class(self->split_deltas[i], "split-delta");
        gtk_widget_set_size_request(self->split_deltas[i], 1, -1);
        gtk_container_add(GTK_CONTAINER(self->split_rows[i]),
            self->split_deltas[i]);

        self->split_times[i] = gtk_label_new(NULL);
        add_class(self->split_times[i], "split-time");
        gtk_widget_set_halign(self->split_times[i], GTK_ALIGN_END);
        gtk_container_add(GTK_CONTAINER(self->split_rows[i]),
            self->split_times[i]);

        if (ls_time_get_by_method(game->split_times[i], game->comparison_method)) {
            ls_split_string(str, ls_time_get_by_method(game->split_times[i], game->comparison_method), 0);
            gtk_label_set_text(GTK_LABEL(self->split_times[i]), str);
        }

        gtk_widget_show_all(self->split_rows[i]);
    }

    // Create group header rows
    if (self->subsplit_data.group_count > 0) {
        self->group_headers = calloc(self->subsplit_data.group_count, sizeof(GroupHeaderWidgets));
        if (!self->group_headers) {
            free_all(self);
            return;
        }

        for (unsigned int g = 0; g < self->subsplit_data.group_count; ++g) {
            const split_group* group = &self->subsplit_data.groups[g];
            GroupHeaderWidgets* h = &self->group_headers[g];

            h->row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            add_class(h->row, "split");
            add_class(h->row, "split-group-header");
            gtk_widget_set_hexpand(h->row, TRUE);

            h->title = gtk_label_new(group->name);
            add_class(h->title, "split-title");
            add_class(h->title, "split-group-header-title");
            gtk_widget_set_halign(h->title, GTK_ALIGN_START);
            gtk_widget_set_hexpand(h->title, TRUE);
            gtk_container_add(GTK_CONTAINER(h->row), h->title);

            h->delta = gtk_label_new(NULL);
            add_class(h->delta, "split-delta");
            add_class(h->delta, "split-group-header-delta");
            gtk_widget_set_size_request(h->delta, 1, -1);
            gtk_container_add(GTK_CONTAINER(h->row), h->delta);

            h->time = gtk_label_new(NULL);
            add_class(h->time, "split-time");
            add_class(h->time, "split-group-header-time");
            gtk_widget_set_halign(h->time, GTK_ALIGN_END);
            gtk_container_add(GTK_CONTAINER(h->row), h->time);

            gtk_container_add(GTK_CONTAINER(self->splits), h->row);
            unsigned int insert_pos = group->start_idx + g;
            gtk_box_reorder_child(GTK_BOX(self->splits), h->row, insert_pos);

            gtk_widget_show_all(h->row);
        }
    }

    if (self->icons_css_provider) {
        // remove old css provider
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(self->icons_css_provider));
        g_object_unref(self->icons_css_provider);
        self->icons_css_provider = NULL;
    }

    if (icons_css_src->len > 0) {
        self->icons_css_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(
            self->icons_css_provider,
            icons_css_src->str,
            icons_css_src->len,
            NULL);
        // add new css provider
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(self->icons_css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_string_free(icons_css_src, TRUE);
    }

    gtk_widget_show(self->splits);
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
    gtk_widget_hide(self->splits);
    gtk_widget_hide(self->split_last);
    for (i = self->split_count - 1; i >= 0; --i) {
        gtk_container_remove(
            GTK_CONTAINER(gtk_widget_get_parent(self->split_rows[i])),
            self->split_rows[i]);
    }
    for (unsigned int g = 0; g < self->subsplit_data.group_count; ++g) {
        gtk_container_remove(
            GTK_CONTAINER(gtk_widget_get_parent(self->group_headers[g].row)),
            self->group_headers[g].row);
    }
    gtk_adjustment_set_value(self->split_adjust, 0);
    free_all(self);
    self->split_count = 0;
    self->subsplit_data.group_count = 0;
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
        // Show/hide subsplit rows based on group activity
        if (self->subsplit_data.split_group_index && self->subsplit_data.split_group_index[i] >= 0) {
            int g = self->subsplit_data.split_group_index[i];
            bool active = group_is_active(&self->subsplit_data.groups[g], timer);
            gtk_widget_set_visible(self->split_rows[i], active);
        }

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
            width = gtk_widget_get_allocated_width(self->split_deltas[i]);
            if (width > delta_width) {
                delta_width = width;
            }
            width = gtk_widget_get_allocated_width(self->split_times[i]);
            if (width > time_width) {
                time_width = width;
            }
        }
        for (unsigned int g = 0; g < self->subsplit_data.group_count; ++g) {
            width = gtk_widget_get_allocated_width(self->group_headers[g].delta);
            if (width > delta_width) {
                delta_width = width;
            }
            width = gtk_widget_get_allocated_width(self->group_headers[g].time);
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
                width = gtk_widget_get_allocated_width(
                    self->split_times[i]);
                gtk_widget_set_margin_start(self->split_times[i],
                    /*WINDOW_PAD*/ 8 * 2 + (time_width - width));
            }
        }
        for (unsigned int g = 0; g < self->subsplit_data.group_count; ++g) {
            if (delta_width) {
                gtk_widget_set_size_request(
                    self->group_headers[g].delta, delta_width, -1);
            }
            if (time_width) {
                width = gtk_widget_get_allocated_width(
                    self->group_headers[g].time);
                gtk_widget_set_margin_start(self->group_headers[g].time,
                    8 * 2 + (time_width - width));
            }
        }
    }

    // Update group header times and deltas

    for (unsigned int g = 0; g < self->subsplit_data.group_count; ++g) {
        const split_group* group = &self->subsplit_data.groups[g];
        bool group_active = group_is_active(group, timer);
        const ls_time_method method = game->comparison_method;

        remove_class(self->group_headers[g].time, "time");
        remove_class(self->group_headers[g].time, "done");
        long long display_time = 0;

        if (group_active) {
            if (is_time_valid(ls_time_get_by_method(game->split_times[group->end_idx], method))) {
                display_time = ls_time_get_by_method(game->split_times[group->end_idx], method);
                if (group->start_idx > 0
                    && is_time_valid(ls_time_get_by_method(game->split_times[group->start_idx - 1], method))) {
                    display_time -= ls_time_get_by_method(game->split_times[group->start_idx - 1], method);
                }
            }
        } else {
            if (timer->curr_split > group->end_idx
                && is_time_valid(ls_time_get_by_method(timer->split_times[group->end_idx], method))) {
                display_time = ls_time_get_by_method(timer->split_times[group->end_idx], method);
                add_class(self->group_headers[g].time, "done");
            } else if (is_time_valid(ls_time_get_by_method(game->split_times[group->end_idx], method))) {
                display_time = ls_time_get_by_method(game->split_times[group->end_idx], method);
            }
        }

        if (display_time > 0 && display_time < LLONG_MAX) {
            add_class(self->group_headers[g].time, "time");
            ls_split_string(str, display_time, 0);
            gtk_label_set_text(GTK_LABEL(self->group_headers[g].time), str);
        } else {
            gtk_label_set_text(GTK_LABEL(self->group_headers[g].time), "");
        }

        remove_class(self->group_headers[g].delta, "behind");
        remove_class(self->group_headers[g].delta, "delta");
        remove_class(self->group_headers[g].delta, "subsplit-progress");
        gtk_label_set_text(GTK_LABEL(self->group_headers[g].delta), "");

        if (group_active
            && timer->started
            && is_time_valid(ls_time_get_by_method(timer->split_times[timer->curr_split], method))) {
            long long progress = ls_time_get_by_method(timer->split_times[timer->curr_split], method);
            if (group->start_idx > 0
                && is_time_valid(ls_time_get_by_method(timer->split_times[group->start_idx - 1], method))) {
                progress -= ls_time_get_by_method(timer->split_times[group->start_idx - 1], method);
            }
            if (progress > 0) {
                add_class(self->group_headers[g].delta, "subsplit-progress");
                ls_split_string(str, progress, 0);
                gtk_label_set_text(GTK_LABEL(self->group_headers[g].delta), str);
            }
        } else if (timer->curr_split > group->end_idx && display_time > 0) {
            long long pb_cum = ls_time_get_by_method(game->split_times[group->end_idx], method);
            long long delta = display_time - pb_cum;
            if (delta > 0)
                add_class(self->group_headers[g].delta, "behind");
            if (delta) {
                add_class(self->group_headers[g].delta, "delta");
                ls_delta_string(str, delta);
                gtk_label_set_text(GTK_LABEL(self->group_headers[g].delta), str);
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
    int split_x, split_y;
    int split_h;
    int scroller_h;
    double curr_scroll;
    double min_scroll, max_scroll;

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
    gtk_widget_translate_coordinates(
        self->split_titles[prev],
        self->split_viewport,
        0, 0, &split_x, &split_y);
    scroller_h = gtk_widget_get_allocated_height(self->split_scroller);
    split_h = gtk_widget_get_allocated_height(self->split_titles[prev]);
    if (curr != next && curr != prev) {
        split_h += gtk_widget_get_allocated_height(self->split_titles[curr]);
    }
    if (next != prev) {
        int h = gtk_widget_get_allocated_height(self->split_titles[next]);
        if (split_h + h < scroller_h) {
            split_h += h;
        }
    }
    min_scroll = split_y + curr_scroll - scroller_h + split_h;
    max_scroll = split_y + curr_scroll;
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
