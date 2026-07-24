/** \file split_groups.h
 *
 * Types and declarations for subsplit group functionality.
 */
#ifndef __SPLIT_GROUPS_H__
#define __SPLIT_GROUPS_H__

#include "splits.h"

/**
 * @brief Describes one subsplit group: a consecutive run of indented
 *        splits ending with a group-ender.
 */
typedef struct split_group {
    char* name;
    unsigned int start_idx;
    unsigned int end_idx;
} split_group;

/**
 * Parses subsplit prefixes and builds subsplit groups.
 *
 * On success, fills in split_display_titles, split_is_subsplit,
 * split_group_index, groups, and group_count.
 *
 * @param self The splits component.
 * @param game The game struct (for split_titles).
 * @return 0 on success, -1 on allocation failure.
 */
int parse_subsplits(LSSplits* self, const ls_game* game);

/**
 * Creates group header widget rows and inserts them into the splits box.
 *
 * @param self The splits component.
 * @return 0 on success, -1 on allocation failure.
 */
int create_group_headers(LSSplits* self);

/**
 * Updates group header times and deltas based on timer state.
 *
 * @param self The splits component.
 * @param game The game struct.
 * @param timer The timer instance.
 */
void draw_group_headers(LSSplits* self, const ls_game* game,
    const ls_timer* timer);

#endif /* __SPLIT_GROUPS_H__ */
