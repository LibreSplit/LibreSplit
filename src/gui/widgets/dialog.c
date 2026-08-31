#include "dialog.h"
#include "src/logging.h"

typedef struct {
    gatomicrefcount references;
    GWeakRef parent;
    GtkWindow* window;
    char* title;
    char* message;
    char* detail;
    LSDialogIcon* icon;
    LSDialogOption* options;
    gsize options_count;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
    gulong destroy_handler;
    int cancel_button;
    gboolean completed;
} LSDialogRequest;

static void g_icon_free(LSDialogIcon* icon)
{
    if (icon == NULL) {
        return;
    }

    if (icon->source != NULL) {
        switch (icon->type) {
            case LS_DIALOG_ICON_NAME:
            case LS_DIALOG_ICON_FILE:
            case LS_DIALOG_ICON_RESOURCE:
                g_free(icon->source);

            case LS_DIALOG_ICON_GICON:
            case LS_DIALOG_ICON_PAINTABLE:
                g_object_unref(icon->source);

            default:
                LOG_WARNF("Unsupported or invalid type: %d", icon->type);
        }
    }

    g_free(icon);
}

static gpointer g_icondup(gpointer source, LSDialogIconType type)
{
    switch (type) {
        case LS_DIALOG_ICON_NAME:
        case LS_DIALOG_ICON_FILE:
        case LS_DIALOG_ICON_RESOURCE:
            return g_strdup(source);

        case LS_DIALOG_ICON_GICON:
        case LS_DIALOG_ICON_PAINTABLE:
            return g_object_ref(source);

        default:
            LOG_WARNF("Unsupported or invalid type: %d", type);
    }

    return NULL;
}

static GtkWidget* get_icon_widget_from_ls_icon(LSDialogIcon* icon)
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
    g_icon_free(request->icon);
    g_weak_ref_clear(&request->parent);
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
    if (response >= 0 && response < request->options_count) {
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
    int response = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "ls-dialog-response"));
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

    GtkWidget* icon = get_icon_widget_from_ls_icon(request->icon);
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

gboolean ls_dialog_open(GtkWindow* parent,
    const char* title,
    const char* message,
    const char* detail,
    const LSDialogOption* options,
    const LSDialogIcon* icon,
    gsize options_count,
    gpointer user_data,
    GDestroyNotify user_data_destroy)
{
    if (parent == NULL) {
        LOG_ERR("Invalid ls_dialog_open usage: parent was null");
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
        LOG_ERR("Invalid ls_dialog_open usage: options_count exceeded limits");
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
    request->options = g_new(LSDialogOption, options_count);
    request->options_count = options_count;
    request->user_data = user_data;
    request->user_data_destroy = user_data_destroy;
    request->cancel_button = cancel_button;
    request->completed = FALSE;

    if (icon != NULL) {
        request->icon = g_new(LSDialogIcon, 1);
        request->icon->source = g_icondup(icon->source, icon->type);
        request->icon->type = icon->type;
    }

    for (gsize i = 0; i < options_count; i++) {
        request->options[i] = options[i];
        request->options[i].label = g_strdup(options[i].label);
    }

    g_idle_add_full(G_PRIORITY_DEFAULT, dialog_present, g_steal_pointer(&request), dialog_request_unref);
    return TRUE;
}