#include "dialog.h"
#include "src/gui/app_window.h"
#include "src/gui/backends/x11.h"
#include "src/logging.h"
#include <stdatomic.h>
#include <sys/stat.h>

/**
 * @brief The request object to construct when requesting a new dialog.
 * This copies all of the caller's data into persistent memory and is responsible
 * for storing messages, icons, options/callbacks, memory free operations etc.
 *
 * For internal use only.
 */
typedef struct {
    gatomicrefcount references;
    GWeakRef parent;
    gboolean has_parent;
    GtkWindow* window;
    char* title;
    char* message;
    char* detail;
    LSDialogIcon* icon;
    void (*icon_free)(gpointer source);
    LSDialogOption* options;
    gsize options_count;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
    gulong destroy_handler;
    int cancel_button;
    gboolean completed;
} LSDialogRequest;

/**
 * @brief The request object to construct when requesting a new file picker.
 * This copies all of the caller's data into persistent memory and is responsible
 * for storing strings, callback, filters etc.
 *
 * For internal use only.
 */
typedef struct {
    gatomicrefcount references;
    GWeakRef parent;
    char* title;
    char* path;
    LSFilePickerFilter* filters;
    gsize filters_count;
    LSFileSelectedCallback callback;
} LSFilePickerRequest;

static atomic_uint dialog_count;

/**
 * @brief x11 workaround, dialogs do not inherit their parent's wm_state. As a workaround for x11
 * we disable keep on top temporarily while a dialog is open in order to allow the dialog to appear
 * above the main window. This should not interfer with the intent of the actual user's
 * keep on top preference since the window will be in focus during dialog interactions and restored
 * after the dialog is dismissed.
 *
 * @param setting The setting value for whether to keep on top or not
 */
static void set_main_window_keep_above(gboolean setting)
{
    if (!is_x11_display()) {
        return;
    }

    GApplication* app = g_application_get_default();
    if (!GTK_IS_APPLICATION(app)) {
        return;
    }

    LSAppWindow* win = ls_get_main_app_window(GTK_APPLICATION(app));
    if (win != NULL && win->opts.win_on_top) {
        x11_set_keep_above(GTK_WINDOW(win), setting);
    }
}

static gboolean restore_main_window_keep_above(gpointer data)
{
    if (atomic_load(&dialog_count) == 0) {
        set_main_window_keep_above(TRUE);
    }

    return G_SOURCE_REMOVE;
}

static void dialog_count_dec(void)
{
    // atomic_fetch_sub returns the value BEFORE the subtraction.
    if (atomic_fetch_sub(&dialog_count, 1) == 1) {
        g_idle_add(restore_main_window_keep_above, NULL);
    }
}

/**
 * @brief Returns a bool value of whether or not any number of dialogs are open.
 *
 * @return bool whether or not any dialogs are displayed
 */
bool ls_dialog_exists(void)
{
    return atomic_load(&dialog_count) > 0;
}

/**
 * @brief Ensures the icon supplied is valid for the source/type combination.
 * A NULL LSDialogIcon is valid since this means no icon needs to be presented.
 *
 * @param icon The requested icon resource and type
 * @return gboolean Whether or not the icon is valid
 */
static gboolean is_valid_icon(const LSDialogIcon* icon)
{
    if (icon == NULL) {
        return TRUE;
    }

    if (icon->type < 0 || icon->type >= LS_DIALOG_ICON_INVALID) {
        LOG_ERRF("Invalid icon type supplied: %d", icon->type);
        return FALSE;
    }

    if (icon->source == NULL) {
        LOG_ERR("Invalid icon source was NULL");
        return FALSE;
    }

    if (icon->type == LS_DIALOG_ICON_GICON) {
        if (!G_IS_ICON(icon->source)) {
            LOG_ERR("Invalid GIcon icon source was not a GIcon");
            return FALSE;
        }
    } else if (icon->type == LS_DIALOG_ICON_PAINTABLE) {
        if (!GDK_IS_PAINTABLE(icon->source)) {
            LOG_ERR("Invalid GdkPaintable icon source was not a GdkPaintaible");
            return FALSE;
        }
    } else {
        const char* resource = icon->source;
        if (resource[0] == '\0') {
            LOG_ERR("Invalid string icon source was empty");
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Handles duplicating the icon resource in persistent memory, and assigning its appropriate free method
 * for the icon once the dialog is destroyed.
 *
 * @param request The full dialog request
 * @param icon The icon
 */
static void g_icondup(LSDialogRequest* request, const LSDialogIcon* icon)
{
    switch (icon->type) {
        case LS_DIALOG_ICON_NAME:
        case LS_DIALOG_ICON_FILE:
        case LS_DIALOG_ICON_RESOURCE:
            request->icon->source = g_strdup(icon->source);
            request->icon_free = g_free;
            break;

        case LS_DIALOG_ICON_GICON:
        case LS_DIALOG_ICON_PAINTABLE:
            request->icon->source = g_object_ref(icon->source);
            request->icon_free = g_object_unref;
            break;

        // For completeness but only a valid icon should have made it to this point after `is_valid_icon`
        default:
            LOG_WARNF("Unsupported or invalid type: %d", icon->type);
    }
}

/**
 * @brief Build a new GtkWidget appropriate for the icon type
 *
 * @param icon The LSDialogIcon
 * @return GtkWidget*
 */
static GtkWidget* get_icon_widget(LSDialogIcon* icon)
{
    if (icon == NULL || icon->source == NULL) {
        return NULL;
    }

    switch (icon->type) {
        case LS_DIALOG_ICON_NAME:
            return gtk_image_new_from_icon_name(icon->source);
        case LS_DIALOG_ICON_FILE:
            return gtk_image_new_from_file(icon->source);
        case LS_DIALOG_ICON_RESOURCE:
            return gtk_image_new_from_resource(icon->source);
        case LS_DIALOG_ICON_GICON:
            return gtk_image_new_from_gicon(icon->source);
        case LS_DIALOG_ICON_PAINTABLE:
            return gtk_image_new_from_paintable(icon->source);

        // For completeness but only a valid icon should have made it to this point after `is_valid_icon`
        default:
            LOG_WARNF("Unsupported or invalid type: %d", icon->type);
    }

    return NULL;
}

static void dialog_request_free(LSDialogRequest* request)
{
    if (request->user_data_destroy != NULL) {
        request->user_data_destroy(request->user_data);
    }

    for (gsize i = 0; i < request->options_count; i++) {
        g_free((gpointer)request->options[i].label);
    }

    g_free(request->options);
    g_free(request->title);
    g_free(request->message);
    g_free(request->detail);
    g_weak_ref_clear(&request->parent);

    if (request->icon) {
        if (request->icon_free) {
            request->icon_free(request->icon->source);
        }

        g_free(request->icon);
    }

    g_free(request);
    dialog_count_dec();
}

static LSDialogRequest* dialog_request_ref(LSDialogRequest* request)
{
    g_atomic_ref_count_inc(&request->references);
    return request;
}

static void dialog_request_unref(gpointer data)
{
    LSDialogRequest* request = data;
    if (g_atomic_ref_count_dec(&request->references)) {
        dialog_request_free(request);
    }
}

static void dialog_window_destroyed(GtkWidget* window, gpointer user_data)
{
    LSDialogRequest* request = user_data;
    request->window = NULL;
    request->completed = TRUE;
    dialog_request_unref(request);
}

static void dialog_complete(LSDialogRequest* request, int response)
{
    if (request->completed) {
        return;
    }

    request->completed = true;

    LSDialogCallback callback = NULL;
    if (response >= 0 && response < (int)request->options_count) {
        callback = request->options[response].callback;
    }

    GtkWindow* window = request->window;
    request->window = NULL;
    g_signal_handler_disconnect(window, request->destroy_handler);
    request->destroy_handler = 0;
    gtk_window_destroy(window);

    if (callback != NULL) {
        callback(request->user_data);
    }

    dialog_request_unref(request);
}

static void dialog_button_clicked(GtkButton* button, gpointer user_data)
{
    LSDialogRequest* request = user_data;
    gpointer button_data = g_object_get_data(G_OBJECT(button), "ls-dialog-response");
    int response = GPOINTER_TO_INT(button_data);
    dialog_complete(request, response);
}

static gboolean dialog_close_requested(GtkWindow* window, gpointer user_data)
{
    LSDialogRequest* request = user_data;
    dialog_complete(request, request->cancel_button);
    return TRUE;
}

static gboolean dialog_key_pressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType type, gpointer user_data)
{
    if (keyval != GDK_KEY_Escape) {
        return FALSE;
    }

    LSDialogRequest* request = user_data;
    dialog_complete(request, request->cancel_button);
    return TRUE;
}

static GtkWidget* dialog_label_new(const char* text)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    return label;
}

/**
 * @brief Builds the actual dialog once GTK is ready to run the dialog function on the main thread.
 * It is required for all of the GUI elements to be constructed in the main thread, therefore all of this work
 * must happen once GTK calls `dialog_present` and never in `ls_dialog_open`
 *
 * @param user_data The LSDialogRequest
 * @return gboolean Whether or not this method needs to remain in the GTK queue for continued calling - always false/G_SOURCE_REMOVE
 */
static gboolean dialog_present(gpointer user_data)
{
    LSDialogRequest* request = user_data;
    GtkWindow* parent = GTK_WINDOW(g_weak_ref_get(&request->parent));
    if (request->has_parent && (parent == NULL || gtk_widget_in_destruction(GTK_WIDGET(parent)))) {
        g_clear_object(&parent);
        return G_SOURCE_REMOVE;
    }

    set_main_window_keep_above(FALSE);

    GtkApplication* application = NULL;
    GtkWindow* window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, request->title);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_resizable(window, FALSE);

    if (request->has_parent) {
        gtk_window_set_transient_for(window, parent);
        gtk_window_set_destroy_with_parent(window, TRUE);
        application = gtk_window_get_application(parent);
    }

    if (application == NULL) {
        GApplication* app = g_application_get_default();
        if (GTK_IS_APPLICATION(app)) {
            application = GTK_APPLICATION(app);
        }
    }

    if (application != NULL) {
        gtk_window_set_application(window, application);
    }

    request->window = window;
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(content, 18);
    gtk_widget_set_margin_bottom(content, 18);
    gtk_widget_set_margin_start(content, 18);
    gtk_widget_set_margin_end(content, 18);
    gtk_widget_set_size_request(content, 360, -1);
    gtk_window_set_child(window, content);

    GtkWidget* body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_append(GTK_BOX(content), body);

    GtkWidget* icon = get_icon_widget(request->icon);
    if (icon != NULL) {
        gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
        gtk_widget_set_valign(icon, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(body), icon);
    }

    GtkWidget* text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_widget_set_valign(text, request->detail != NULL && request->detail[0] != '\0' ? GTK_ALIGN_START : GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(body), text);

    GtkWidget* message_label = dialog_label_new(request->message);
    gtk_widget_add_css_class(message_label, "heading");
    gtk_box_append(GTK_BOX(text), message_label);

    if (request->detail != NULL) {
        gtk_box_append(GTK_BOX(text), dialog_label_new(request->detail));
    }

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    gtk_widget_set_margin_top(actions, 10);
    gtk_box_append(GTK_BOX(content), actions);

    GtkWidget* default_widget = NULL;
    for (gsize i = 0; i < request->options_count; i++) {
        GtkWidget* button = gtk_button_new_with_mnemonic(request->options[i].label);
        gtk_widget_set_size_request(button, 96, -1);
        g_object_set_data(G_OBJECT(button), "ls-dialog-response", GINT_TO_POINTER(i));
        g_signal_connect(button, "clicked", G_CALLBACK(dialog_button_clicked), request);
        gtk_box_append(GTK_BOX(actions), button);

        if (request->options[i].is_default) {
            default_widget = button;
            gtk_widget_add_css_class(button, "suggested-action");
            gtk_window_set_default_widget(window, button);
        }
    }

    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(dialog_key_pressed), request);
    gtk_widget_add_controller(GTK_WIDGET(window), key_controller);

    g_signal_connect(window, "close-request", G_CALLBACK(dialog_close_requested), request);
    dialog_request_ref(request);
    request->destroy_handler = g_signal_connect(window, "destroy", G_CALLBACK(dialog_window_destroyed), request);

    gtk_window_present(window);
    if (default_widget != NULL) {
        gtk_widget_grab_focus(default_widget);
    }

    if (request->has_parent) {
        g_object_unref(parent);
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Queues a new dialog to be created by the main GTK Thread.
 * All of the data passed in must be valid at the time of calling.
 * We then copy all of the data to persistent memory to pass the dialog request
 * along to the GTK queue. user_data is not copied, this is expected to be a pointer
 * to in memory-data, not a shallow stack reference. user_data_destroy is supplied
 * for freeing user_data once the dialog is destroyed if necessary.
 *
 * If this function returns FALSE, the caller is responsible for freeing user_data.
 *
 * @param parent The parent window that owns the dialog - NULL for no parent
 * @param title The title displayed on the dialog's titlebar
 * @param message A message header for the dialog shown above the main message body
 * @param detail The main message to show in the dialog
 * @param icon An optional icon to present in the dialog.
 * @param options Options to display in the dialog. Each option represents an action the user can take with an optional callback.
 * @param options_count The number of options in the options array. i.e. G_N_ELEMENTS for a local array.
 * @param user_data Optional user_data to pass to callbacks.
 * @param user_data_destroy Any memory free method to call on user_data during resource destruction for the dialog.
 * @return gboolean Whether or not the request was valid and successfully queue'd for presentation
 */
gboolean ls_dialog_open(GtkWindow* parent,
    const char* title,
    const char* message,
    const char* detail,
    const LSDialogIcon* icon,
    const LSDialogOption* options,
    gsize options_count,
    gpointer user_data,
    GDestroyNotify user_data_destroy)
{
    if (parent != NULL && !GTK_IS_WINDOW(parent)) {
        LOG_ERR("Invalid ls_dialog_open usage: parent was not a valid GTK Window");
        return FALSE;
    }

    if (title == NULL) {
        LOG_ERR("Invalid ls_dialog_open usage: title was null");
        return FALSE;
    }

    if (message == NULL) {
        LOG_ERR("Invalid ls_dialog_open usage: message was null");
        return FALSE;
    }

    if (options == NULL) {
        LOG_ERR("Invalid ls_dialog_open usage: options was null");
        return FALSE;
    }

    if (options_count <= 0 || options_count > MAX_BUTTONS) {
        LOG_ERR("Invalid ls_dialog_open usage: number of options exceeded limits");
        return FALSE;
    }

    if (!is_valid_icon(icon)) {
        return FALSE;
    }

    int cancel_button = -1;
    int default_button = -1;

    for (gsize i = 0; i < options_count; i++) {
        if (options[i].label == NULL || options[i].label[0] == '\0') {
            return FALSE;
        }

        if (options[i].is_cancel) {
            if (cancel_button != -1) {
                return FALSE;
            }

            cancel_button = i;
        }

        if (options[i].is_default) {
            if (default_button != -1) {
                return FALSE;
            }

            default_button = i;
        }
    }

    LSDialogRequest* request = g_new(LSDialogRequest, 1);
    g_atomic_ref_count_init(&request->references);
    request->has_parent = parent != NULL;
    g_weak_ref_init(&request->parent, parent != NULL ? G_OBJECT(parent) : NULL);
    request->title = g_strdup(title);
    request->message = g_strdup(message);
    request->detail = g_strdup(detail);
    request->icon = NULL;
    request->icon_free = NULL;
    request->options = g_new(LSDialogOption, options_count);
    request->options_count = options_count;
    request->user_data = user_data;
    request->user_data_destroy = user_data_destroy;
    request->cancel_button = cancel_button;
    request->completed = FALSE;

    if (icon != NULL) {
        request->icon = g_new(LSDialogIcon, 1);
        request->icon->type = icon->type;
        g_icondup(request, icon);
    }

    for (gsize i = 0; i < options_count; i++) {
        request->options[i] = options[i];
        request->options[i].label = g_strdup(options[i].label);
    }

    atomic_fetch_add(&dialog_count, 1);
    g_idle_add_full(G_PRIORITY_DEFAULT, dialog_present, g_steal_pointer(&request), dialog_request_unref);
    return TRUE;
}

static void file_picker_request_free(LSFilePickerRequest* request)
{
    g_weak_ref_clear(&request->parent);
    g_free(request->title);
    g_free(request->path);

    if (request->filters_count > 0) {
        for (gsize i = 0; i < request->filters_count; i++) {
            g_free((gpointer)request->filters[i].name);
            g_free((gpointer)request->filters[i].pattern);
        }

        g_free(request->filters);
    }

    g_free(request);
    dialog_count_dec();
}

static LSFilePickerRequest* file_picker_request_ref(LSFilePickerRequest* request)
{
    g_atomic_ref_count_inc(&request->references);
    return request;
}

static void file_picker_request_unref(gpointer data)
{
    LSFilePickerRequest* request = data;
    if (g_atomic_ref_count_dec(&request->references)) {
        file_picker_request_free(request);
    }
}

static void file_picker_finish(GObject* diag, GAsyncResult* result, gpointer data)
{
    GError* error = NULL;
    LSFilePickerRequest* request = data;
    GtkFileDialog* dialog = GTK_FILE_DIALOG(diag);
    GFile* file = gtk_file_dialog_open_finish(dialog, result, &error);
    GtkWindow* parent = GTK_WINDOW(g_weak_ref_get(&request->parent));

    if (file != NULL) {
        if (parent != NULL && !gtk_widget_in_destruction(GTK_WIDGET(parent))) {
            char* path = g_file_get_path(file);
            if (path != NULL) {
                request->callback(parent, path);
                g_free(path);
            } else {
                LOG_WARN("Selected file did not have a local path");
            }
        }
    } else if (error != NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
            LOG_WARNF("Failed to open file: %s", error->message);
        }
    } else {
        LOG_WARN("File selection returned no file");
    }

    g_clear_object(&parent);
    g_clear_object(&file);
    g_clear_error(&error);
    file_picker_request_unref(request);
}

/**
 * @brief Builds the actual file picker once GTK is ready to run the function on the main thread.
 * It is required for all of the GUI elements to be constructed in the main thread, therefore all of this work
 * must happen once GTK calls `file_picker_present` and never in `ls_file_picker_open`
 *
 * @param user_data The LSFilePickerRequest
 * @return gboolean Whether or not this method needs to remain in the GTK queue for continued calling - always false/G_SOURCE_REMOVE
 */
static gboolean file_picker_present(gpointer user_data)
{
    LSFilePickerRequest* request = user_data;
    GtkWindow* parent = GTK_WINDOW(g_weak_ref_get(&request->parent));
    if (parent == NULL || gtk_widget_in_destruction(GTK_WIDGET(parent))) {
        g_clear_object(&parent);
        return G_SOURCE_REMOVE;
    }

    set_main_window_keep_above(FALSE);

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, request->title);
    gtk_file_dialog_set_modal(dialog, TRUE);

    GtkFileFilter** allocated_filters = NULL;
    GListStore* filters = NULL;
    GtkFileFilter* default_filter = NULL;

    if (request->filters_count > 0) {
        allocated_filters = g_new0(GtkFileFilter*, request->filters_count);
        filters = g_list_store_new(GTK_TYPE_FILE_FILTER);

        for (gsize i = 0; i < request->filters_count; i++) {
            GtkFileFilter* filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, request->filters[i].name);
            gtk_file_filter_add_pattern(filter, request->filters[i].pattern);
            g_list_store_append(filters, filter);
            allocated_filters[i] = filter;

            if (request->filters[i].is_default) {
                default_filter = filter;
            }
        }

        gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
        if (default_filter != NULL) {
            gtk_file_dialog_set_default_filter(dialog, default_filter);
        }

        g_object_unref(filters);
    }

    GFile* folder = g_file_new_for_path(request->path);
    gtk_file_dialog_set_initial_folder(dialog, folder);
    g_object_unref(folder);

    gtk_file_dialog_open(dialog, parent, NULL, file_picker_finish, file_picker_request_ref(request));

    if (request->filters_count > 0) {
        for (gsize i = 0; i < request->filters_count; i++) {
            g_object_unref(allocated_filters[i]);
        }

        g_free(allocated_filters);
    }

    g_object_unref(dialog);
    g_object_unref(parent);

    return G_SOURCE_REMOVE;
}

/**
 * @brief Queues a file picker dialog to be created by the main GTK Thread.
 * All of the data passed in must be valid at the time of calling.
 * We then copy all of the data to persistent memory to pass the dialog request
 * along to the GTK queue.
 *
 * @param parent The window that owns the file picker - required
 * @param options The file picker config - all fields other than filters are required
 * @param callback Callback for a successful local file selection - required
 * @return gboolean Whether or not the request was valid and successfully queue'd for presentation
 */
gboolean ls_file_picker_open(GtkWindow* parent, const LSFilePickerOptions* options, LSFileSelectedCallback callback)
{
    if (parent == NULL || !GTK_IS_WINDOW(parent)) {
        LOG_ERR("Invalid ls_file_picker_open usage: parent was NULL or not a valid GTK Window");
        return FALSE;
    }

    if (options == NULL) {
        LOG_ERR("Invalid ls_file_picker_open usage: options were null");
        return FALSE;
    }

    if (options->title == NULL || options->title[0] == '\0') {
        LOG_ERR("Invalid ls_file_picker_open usage: title was null or empty");
        return FALSE;
    }

    if (options->path == NULL || options->path[0] == '\0') {
        LOG_ERR("Invalid ls_file_picker_open usage: path was null or empty");
        return FALSE;
    }

    struct stat st = { 0 };
    if (stat(options->path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        LOG_ERR("Invalid ls_file_picker_open usage: path was not a valid system folder path")
        return FALSE;
    }

    if (callback == NULL) {
        LOG_ERR("Invalid ls_file_picker_open usage: callback was null");
        return FALSE;
    }

    if (options->filters_count < 0 || options->filters_count > MAX_FILTERS) {
        LOG_ERR("Invalid ls_file_picker_open usage: number of filters out of bounds");
        return FALSE;
    }

    if (options->filters == NULL && options->filters_count != 0) {
        LOG_ERR("Invalid ls_file_picker_open usage: filters was NULL with a positive filters_count");
        return FALSE;
    }

    bool has_default = false;
    for (gsize i = 0; i < options->filters_count; i++) {
        LSFilePickerFilter filter = options->filters[i];
        if (filter.name == NULL || filter.name[0] == '\0') {
            LOG_ERRF("Invalid ls_file_picker_open usage: filter[%zu].name was null or empty", i);
            return FALSE;
        }

        if (filter.pattern == NULL || filter.pattern[0] == '\0') {
            LOG_ERRF("Invalid ls_file_picker_open usage: filter[%zu].pattern was null or empty", i);
            return FALSE;
        }

        if (filter.is_default) {
            if (has_default) {
                LOG_ERRF("Invalid ls_file_picker_open usage: filter[%zu].is_default multiple default filters defined", i);
                return FALSE;
            }

            has_default = true;
        }
    }

    LSFilePickerRequest* request = g_new(LSFilePickerRequest, 1);
    g_atomic_ref_count_init(&request->references);
    g_weak_ref_init(&request->parent, G_OBJECT(parent));
    request->title = g_strdup(options->title);
    request->path = g_strdup(options->path);
    request->callback = callback;
    request->filters = options->filters_count > 0 ? g_new(LSFilePickerFilter, options->filters_count) : NULL;
    request->filters_count = options->filters_count;

    for (gsize i = 0; i < options->filters_count; i++) {
        LSFilePickerFilter filter = options->filters[i];
        request->filters[i].name = g_strdup(filter.name);
        request->filters[i].pattern = g_strdup(filter.pattern);
        request->filters[i].is_default = filter.is_default;
    }

    atomic_fetch_add(&dialog_count, 1);
    g_idle_add_full(G_PRIORITY_DEFAULT, file_picker_present, request, file_picker_request_unref);
    return TRUE;
}
