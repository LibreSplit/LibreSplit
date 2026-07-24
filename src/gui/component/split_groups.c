/** \file split_groups.c
 *
 * Implementation of subsplit group parsing, rendering, and drawing.
 */
#include "split_groups.h"
#include <gtk/gtk.h>
#include <limits.h>

int parse_subsplits(LSSplits* self, const ls_game* game)
{
    char** temp_group_names = NULL;
    int err = 0;

    self->split_display_titles = calloc(self->split_count, sizeof(char*));
    self->split_is_subsplit = calloc(self->split_count, sizeof(bool));
    self->split_group_index = calloc(self->split_count, sizeof(int));
    temp_group_names = calloc(self->split_count, sizeof(char*));

    if (!self->split_display_titles || !self->split_is_subsplit
        || !self->split_group_index || !temp_group_names) {
        err = -1;
        goto cleanup;
    }
    for (unsigned int i = 0; i < self->split_count; ++i)
        self->split_group_index[i] = -1;

    // First pass: parse prefixes and extract display names
    for (unsigned int i = 0; i < self->split_count; ++i) {
        const char* title = game->split_titles[i];
        if (!title || !title[0]) {
            self->split_display_titles[i] = strdup("");
            continue;
        }
        if (title[0] == '-') {
            const char* p = title + 1;
            while (*p == ' ')
                ++p;
            self->split_display_titles[i] = strdup(p);
            if (!self->split_display_titles[i]) {
                err = -1;
                goto cleanup;
            }
            self->split_is_subsplit[i] = true;
        } else if (title[0] == '{') {
            const char* close = strchr(title, '}');
            if (close) {
                temp_group_names[i] = strndup(title + 1, close - title - 1);
                if (!temp_group_names[i]) {
                    err = -1;
                    goto cleanup;
                }
                const char* p = close + 1;
                while (*p == ' ')
                    ++p;
                self->split_display_titles[i] = strdup(*p ? p : close + 1);
                if (!self->split_display_titles[i]) {
                    err = -1;
                    goto cleanup;
                }
            } else {
                self->split_display_titles[i] = strdup(title);
                if (!self->split_display_titles[i]) {
                    err = -1;
                    goto cleanup;
                }
            }
            self->split_is_subsplit[i] = true;
        } else {
            self->split_display_titles[i] = strdup(title);
            if (!self->split_display_titles[i]) {
                err = -1;
                goto cleanup;
            }
            self->split_is_subsplit[i] = false;
        }
    }

    // Count groups (splits with both a group name and subsplit flag)
    self->group_count = 0;
    for (unsigned int i = 0; i < self->split_count; ++i) {
        if (temp_group_names[i] && self->split_is_subsplit[i])
            ++self->group_count;
    }

    if (self->group_count > 0) {
        self->groups = calloc(self->group_count, sizeof(split_group));
        if (!self->groups) {
            err = -1;
            goto cleanup;
        }

        // Second pass: build group objects, assign split_group_index
        unsigned int g = 0;
        int group_start = -1;
        for (unsigned int i = 0; i < self->split_count; ++i) {
            if (self->split_is_subsplit[i]) {
                if (group_start == -1)
                    group_start = i;
                if (temp_group_names[i]) {
                    self->groups[g].name = temp_group_names[i];
                    temp_group_names[i] = NULL;
                    self->groups[g].start_idx = group_start;
                    self->groups[g].end_idx = i;
                    for (unsigned int s = group_start; s <= i; ++s)
                        self->split_group_index[s] = g;
                    ++g;
                    group_start = -1;
                }
            } else {
                group_start = -1;
            }
        }
    }

cleanup:
    if (temp_group_names) {
        for (unsigned int i = 0; i < self->split_count; ++i)
            free(temp_group_names[i]);
        free(temp_group_names);
    }
    return err;
}

int create_group_headers(LSSplits* self)
{
    if (self->group_count == 0)
        return 0;

    self->group_header_rows = calloc(self->group_count, sizeof(GtkWidget*));
    self->group_header_titles = calloc(self->group_count, sizeof(GtkWidget*));
    self->group_header_times = calloc(self->group_count, sizeof(GtkWidget*));
    self->group_header_deltas = calloc(self->group_count, sizeof(GtkWidget*));
    if (!self->group_header_rows || !self->group_header_titles
        || !self->group_header_times || !self->group_header_deltas)
        return -1;

    for (unsigned int g = 0; g < self->group_count; ++g) {
        const split_group* group = &self->groups[g];
        self->group_header_rows[g] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        add_class(self->group_header_rows[g], "split");
        add_class(self->group_header_rows[g], "split-group-header");
        gtk_widget_set_hexpand(self->group_header_rows[g], TRUE);

        self->group_header_titles[g] = gtk_label_new(group->name);
        add_class(self->group_header_titles[g], "split-title");
        add_class(self->group_header_titles[g], "split-group-header-title");
        gtk_widget_set_halign(self->group_header_titles[g], GTK_ALIGN_START);
        gtk_widget_set_hexpand(self->group_header_titles[g], TRUE);
        gtk_container_add(GTK_CONTAINER(self->group_header_rows[g]),
            self->group_header_titles[g]);

        self->group_header_deltas[g] = gtk_label_new(NULL);
        add_class(self->group_header_deltas[g], "split-delta");
        add_class(self->group_header_deltas[g], "split-group-header-delta");
        gtk_widget_set_size_request(self->group_header_deltas[g], 1, -1);
        gtk_container_add(GTK_CONTAINER(self->group_header_rows[g]),
            self->group_header_deltas[g]);

        self->group_header_times[g] = gtk_label_new(NULL);
        add_class(self->group_header_times[g], "split-time");
        add_class(self->group_header_times[g], "split-group-header-time");
        gtk_widget_set_halign(self->group_header_times[g], GTK_ALIGN_END);
        gtk_container_add(GTK_CONTAINER(self->group_header_rows[g]),
            self->group_header_times[g]);

        // Insert header before the first subsplit of the group
        gtk_container_add(GTK_CONTAINER(self->splits),
            self->group_header_rows[g]);
        unsigned int insert_pos = group->start_idx + g;
        gtk_box_reorder_child(GTK_BOX(self->splits),
            self->group_header_rows[g], insert_pos);

        gtk_widget_show_all(self->group_header_rows[g]);
    }
    return 0;
}

void draw_group_headers(LSSplits* self, const ls_game* game,
    const ls_timer* timer)
{
    char str[256];

    for (unsigned int g = 0; g < self->group_count; ++g) {
        const split_group* group = &self->groups[g];
        bool group_active = timer->curr_split >= group->start_idx
            && timer->curr_split <= group->end_idx;

        remove_class(self->group_header_times[g], "time");
        remove_class(self->group_header_times[g], "done");
        long long display_time = 0;

        if (group_active) {
            // Active group: show cumulative PB time for the group (segment sum)
            if (game->split_times[group->end_idx] && game->split_times[group->end_idx] < LLONG_MAX) {
                display_time = game->split_times[group->end_idx];
                if (group->start_idx > 0 && game->split_times[group->start_idx - 1]
                    && game->split_times[group->start_idx - 1] < LLONG_MAX) {
                    display_time -= game->split_times[group->start_idx - 1];
                }
            }
        } else {
            // Completed group: show actual run time at end of group
            if (timer->curr_split > group->end_idx && timer->split_times[group->end_idx]) {
                display_time = timer->split_times[group->end_idx];
                add_class(self->group_header_times[g], "done");
            } else if (game->split_times[group->end_idx] && game->split_times[group->end_idx] < LLONG_MAX) {
                display_time = game->split_times[group->end_idx];
            }
        }

        if (display_time > 0 && display_time < LLONG_MAX) {
            add_class(self->group_header_times[g], "time");
            ls_split_string(str, display_time, 0);
            gtk_label_set_text(GTK_LABEL(self->group_header_times[g]), str);
        } else {
            gtk_label_set_text(GTK_LABEL(self->group_header_times[g]), "");
        }

        // Delta: shown when group is fully completed, or live progress when active
        remove_class(self->group_header_deltas[g], "best-split");
        remove_class(self->group_header_deltas[g], "best-segment");
        remove_class(self->group_header_deltas[g], "behind");
        remove_class(self->group_header_deltas[g], "losing");
        remove_class(self->group_header_deltas[g], "delta");
        remove_class(self->group_header_deltas[g], "subsplit-progress");
        gtk_label_set_text(GTK_LABEL(self->group_header_deltas[g]), "");

        if (group_active && timer->curr_split >= group->start_idx
            && timer->started && timer->split_times[timer->curr_split]) {
            // Show live progress within the group
            long long progress = timer->split_times[timer->curr_split];
            if (group->start_idx > 0 && timer->split_times[group->start_idx - 1])
                progress -= timer->split_times[group->start_idx - 1];
            if (progress > 0) {
                add_class(self->group_header_deltas[g], "subsplit-progress");
                ls_split_string(str, progress, 0);
                gtk_label_set_text(GTK_LABEL(self->group_header_deltas[g]), str);
            }
        } else if (timer->curr_split > group->end_idx && display_time > 0
            && timer->curr_split > 0) {
            long long pb_cum = game->split_times[group->end_idx];
            long long delta = display_time - pb_cum;
            if (delta > 0)
                add_class(self->group_header_deltas[g], "behind");
            else
                remove_class(self->group_header_deltas[g], "behind");
            if (delta) {
                add_class(self->group_header_deltas[g], "delta");
                ls_delta_string(str, delta);
                gtk_label_set_text(GTK_LABEL(self->group_header_deltas[g]), str);
            }
        }
    }
}
