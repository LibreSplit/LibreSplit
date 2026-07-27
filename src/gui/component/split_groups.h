/** \file split_groups.h
 *
 * Types and declarations for subsplit group functionality.
 */
#ifndef __SPLIT_GROUPS_H__
#define __SPLIT_GROUPS_H__

#include <stdbool.h>

struct ls_game;

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
 * @brief Container for subsplit metadata parsed from split titles.
 *
 * Holds display titles, subsplit flags, group indices, and group
 * definitions.  An instance of this struct is embedded in LSSplits.
 */
typedef struct SubsplitData {
    unsigned int group_count;
    char** split_display_titles;
    bool* split_is_subsplit;
    int* split_group_index;
    split_group* groups;
} SubsplitData;

/**
 * Parses subsplit prefixes and builds subsplit groups.
 *
 * On success fills in the subsplit_data fields.
 *
 * @param subsplit The subsplit data struct to fill.
 * @param game     The game struct (for split_titles).
 * @param split_count Number of splits.
 * @return 0 on success, -1 on allocation failure.
 */
int parse_subsplits(SubsplitData* subsplit, const struct ls_game* game,
    unsigned int split_count);

#endif /* __SPLIT_GROUPS_H__ */
