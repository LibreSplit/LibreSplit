#pragma once

#include <glib.h>

/**
 * @brief The LSAppWindow options
 */
typedef struct {
    gboolean hide_cursor; /*!< Defines whether the cursor should be hidden when on top of LibreSplit */
    gboolean global_hotkeys; /*!< Defines whether global hotkeys are currently enabled */
    gboolean decorated; /*!< Defines whether LibreSplit is currently showing window decorations */
    gboolean win_on_top; /*!< Defines whether LibreSplit is currently "always-on-top" */
} LSOpts;
