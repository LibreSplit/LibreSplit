#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MAX_BUTTONS 100
#define MAX_FILTERS 100

/**
 * Callback for handling a dialog response select.
 * Null means that nothing needs to be notified and no action must be taken.
 *
 * @param user_data - Pointer to user supplied data pass to ls_dialog_open
 *
 */
typedef void (*LSDialogCallback)(gpointer user_data);

typedef void (*LSFileSelectedCallback)(GtkWindow* parent, const char* path);

/**
 * @brief Valid supported icon types.
 * To add a new type, you must add a new entry to the enum, then add its implementation
 * in the following functions of dialog.c:
 *
 * is_valid_icon - ensure that the supplied source is a valid source for the type.
 * g_icondup - call the correct allocation method for duplicating the resource in persistent memory and assign to the request
 *             assign the correct resource destruction method to the request's icon_free for your source type
 * get_icon_widget - create a new GtkWidget for the icon that displays your icon and return the GtkWidget.
 */
typedef enum {
    LS_DIALOG_ICON_NAME, /**< Icon from a standard theme icon name */
    LS_DIALOG_ICON_FILE, /**< Icon from a file path */
    LS_DIALOG_ICON_RESOURCE, /**< Icon from a resource path */
    LS_DIALOG_ICON_GICON, /**< Icon from a GIcon pointer */
    LS_DIALOG_ICON_PAINTABLE, /**< Icon from a GdkPaintable pointer */
    LS_DIALOG_ICON_INVALID, /**< Not a valid icon type, this must always be last and is for type checking the enum value */
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

typedef struct {
    const char* name; /**< The name of the filter the user sees */
    const char* pattern; /**< Glob pattern the filter uses */
    gboolean is_default; /**< Whether or not this filter is the default */
} LSFilePickerFilter;

typedef struct {
    const char* title; /**< File picker title */
    const char* path; /**< Initial path that the file picker presents to the user */
    LSFilePickerFilter* filters; /**< Pointer to an array of filters */
    gsize filters_count; /**< The number of filters in the filters array */
} LSFilePickerOptions;

bool ls_dialog_exists();

gboolean ls_dialog_open(GtkWindow* parent,
    const char* title,
    const char* message,
    const char* detail,
    const LSDialogIcon* icon,
    const LSDialogOption* options,
    gsize options_count,
    gpointer user_data,
    GDestroyNotify user_data_destroy);

gboolean ls_file_picker_open(GtkWindow* parent, const LSFilePickerOptions* options, LSFileSelectedCallback callback);

G_END_DECLS
