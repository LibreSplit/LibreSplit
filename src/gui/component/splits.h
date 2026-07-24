/** \file splits.h
 *
 * Shared types for the splits component.
 */
#ifndef __SPLITS_H__
#define __SPLITS_H__

#include "components.h"

struct split_group;

/**
 * @brief The component containing all the splits for the game.
 */
typedef struct LSSplits {
    LSComponent base; /*!< The base struct that is extended */
    unsigned int split_count; /*!< The number of splits */
    unsigned int group_count;
    char** split_display_titles;
    bool* split_is_subsplit;
    int* split_group_index;
    struct split_group* groups;
    GtkWidget* container; /*!< The container for the splits */
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
    GtkWidget** group_header_rows;
    GtkWidget** group_header_titles;
    GtkWidget** group_header_times;
    GtkWidget** group_header_deltas;
    GtkCssProvider* icons_css_provider;
} LSSplits;

#endif /* __SPLITS_H__ */
