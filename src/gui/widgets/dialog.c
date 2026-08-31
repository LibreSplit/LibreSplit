#include "dialog.h"
#include "src/logging.h"

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
 * @param icon
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
    if (parent == NULL || gtk_widget_in_destruction(GTK_WIDGET(parent))) {
        g_clear_object(&parent);
        return G_SOURCE_REMOVE;
    }

    GtkWindow* window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, request->title);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_transient_for(window, parent);
    gtk_window_set_destroy_with_parent(window, TRUE);
    request->window = window;

    GtkApplication* application = gtk_window_get_application(parent);
    if (application != NULL) {
        gtk_window_set_application(window, application);
    }

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

    g_object_unref(parent);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Queues a new dialog to be created by the main GTK Thread.
 * All of the data passed in must be valid at the time of calling.
 * We then copy all of the data to persistent memory to pass the dialog request
 * along to the GTK queue.
 *
 * @param parent The parent window that owns the dialog
 * @param title The title displayed on the dialog's titlebar
 * @param message A message header for the dialog shown above the main message body
 * @param detail The main message to show in the dialog
 * @param icon An optional icon to present in the dialog.
 * @param options Options to display in the dialog. Each option represents an action the user can take with an optional callback.
 * @param options_count The number of options in the options array. Use G_N_ELEMENTS.
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
    if (parent == NULL || !GTK_IS_WINDOW(parent)) {
        LOG_ERR("Invalid ls_dialog_open usage: parent was null or not a valid GTK Window");
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
    g_weak_ref_init(&request->parent, G_OBJECT(parent));
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

    g_idle_add_full(G_PRIORITY_DEFAULT, dialog_present, g_steal_pointer(&request), dialog_request_unref);
    return TRUE;
}