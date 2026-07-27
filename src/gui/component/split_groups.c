/** \file split_groups.c
 *
 * Implementation of subsplit group parsing.
 */
#include "split_groups.h"
#include "../../timer.h"
#include <stdlib.h>
#include <string.h>

int parse_subsplits(SubsplitData* subsplit, const struct ls_game* game,
    unsigned int split_count)
{
    subsplit->split_display_titles = calloc(split_count, sizeof(char*));
    subsplit->split_is_subsplit = calloc(split_count, sizeof(bool));
    subsplit->split_group_index = calloc(split_count, sizeof(int));

    if (!subsplit->split_display_titles || !subsplit->split_is_subsplit
        || !subsplit->split_group_index)
        return -1;

    for (unsigned int i = 0; i < split_count; ++i)
        subsplit->split_group_index[i] = -1;

    subsplit->group_count = 0;
    subsplit->groups = NULL;

    int group_start = -1;

    for (unsigned int i = 0; i < split_count; ++i) {
        const char* title = game->split_titles[i];
        if (!title || !title[0]) {
            subsplit->split_display_titles[i] = strdup("");
            if (!subsplit->split_display_titles[i]) return -1;
            group_start = -1;
            continue;
        }

        if (title[0] == '-') {
            const char* p = title + 1;
            while (*p == ' ')
                ++p;
            subsplit->split_display_titles[i] = strdup(p);
            if (!subsplit->split_display_titles[i]) return -1;
            subsplit->split_is_subsplit[i] = true;
            if (group_start == -1)
                group_start = i;
        } else if (title[0] == '{') {
            const char* close = strchr(title, '}');
            subsplit->split_is_subsplit[i] = true;
            if (close) {
                char* group_name = strndup(title + 1, close - title - 1);
                if (!group_name) return -1;
                const char* p = close + 1;
                while (*p == ' ')
                    ++p;
                subsplit->split_display_titles[i] = strdup(*p ? p : close + 1);
                if (!subsplit->split_display_titles[i]) {
                    free(group_name);
                    return -1;
                }
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
                subsplit->split_display_titles[i] = strdup(title);
                if (!subsplit->split_display_titles[i]) return -1;
            }
        } else {
            subsplit->split_display_titles[i] = strdup(title);
            if (!subsplit->split_display_titles[i]) return -1;
            subsplit->split_is_subsplit[i] = false;
            group_start = -1;
        }
    }

    return 0;
}
