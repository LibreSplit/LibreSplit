#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MAX_BUTTONS 100

/**
 * Callback for handling a dialog response select.
 * Null means that nothing needs to be notified and no action must be taken.
 *
 * @param user_data - Pointer to user supplied data pass to ls_dialog_open
 *
 */
typedef void (*LSDialogCallback)(gpointer user_data);

typedef struct {
    const char* label; /**< button label text */
    LSDialogCallback callback; /**< Action to run on click, or NULL to do nothing/cancel */
    gboolean is_cancel; /**< Whether escape or window close selects this option */
    gboolean is_default; /**< Whether or not this option is the default focus */
} LSDialogOption;

void ls_dialog_set_main_window(GtkWindow* window);

gboolean ls_dialog_open(const char* title,
    const char* message,
    const char* detail,
    const LSDialogOption* options,
    gsize options_count,
    gpointer user_data,
    GDestroyNotify user_data_destroy);

G_END_DECLS