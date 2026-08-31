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

typedef enum {
    LS_DIALOG_ICON_NAME,
    LS_DIALOG_ICON_FILE,
    LS_DIALOG_ICON_RESOURCE,
    LS_DIALOG_ICON_GICON,
    LS_DIALOG_ICON_PAINTABLE,
} LSDialogIconType;

typedef struct {
    gpointer source; /**< The source from which to load the icon image */
    LSDialogIconType type; /**< The icon source type that will determine how it's loaded (i.e. from file) */
} LSDialogIcon;

typedef struct {
    const char* label; /**< button label text */
    LSDialogCallback callback; /**< Action to run on click, or NULL to do nothing/cancel */
    gboolean is_cancel; /**< Whether escape or window close selects this option */
    gboolean is_default; /**< Whether or not this option is the default focus */
} LSDialogOption;

gboolean ls_dialog_open(GtkWindow* parent,
    const char* title,
    const char* message,
    const char* detail,
    const LSDialogOption* options,
    const LSDialogIcon* icon,
    gsize options_count,
    gpointer user_data,
    GDestroyNotify user_data_destroy);

G_END_DECLS