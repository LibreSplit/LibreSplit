#include "utils.h"

typedef struct IconInput {
    char* path;
    char* source;
    char* selector;
} IconInput;

typedef struct IconRequest {
    IconInput* input;
    GtkCssProvider* provider;
    GCancellable* cancellable;
} IconRequest;

static GQueue icon_queue = G_QUEUE_INIT;
static unsigned int icon_active;

/**
 * Adds a styling class to a GTK Widget.
 *
 * @param widget The widget to add the class to
 * @param class The class to add
 */
void add_class(GtkWidget* widget, const char* class)
{
    gtk_widget_add_css_class(widget, class);
}

/**
 * Removes a styling class from a GTK Widget.
 *
 * @param widget The widget to remove the class from
 * @param class The class to remove
 */
void remove_class(GtkWidget* widget, const char* class)
{
    gtk_widget_remove_css_class(widget, class);
}

/**
 * @brief Resolves the icon file path to a URI.
 * This handles converting full/relative file paths to the user's system
 * so that it can be converted to a usable file:// URI
 * while retaining already valid URI's for web urls, data-urls etc.
 *
 * If you use this, you must g_free the result if it is not NULL after usage.
 *
 * @param path The path to the game file.
 * @param source The path to the icon being loaded.
 * @return char* A new string to for the valid icon URI.
 */
char* icon_uri(const char* path, const char* source)
{
    // max_len should be -1 for null terminated strings.
    if (!source || !g_utf8_validate(source, -1, NULL)) {
        return NULL;
    }

    // The supplied source is already correctly formatted.
    // Duplicate it so the followup free doesn't kill the source path.
    if (g_uri_peek_scheme(source)) {
        return g_strdup(source);
    }

    GFile* icon;
    if (g_path_is_absolute(source)) {
        icon = g_file_new_for_path(source);
    } else {
        GFile* splits_file = g_file_new_for_path(path);
        GFile* parent_dir = g_file_get_parent(splits_file);
        g_object_unref(splits_file);

        if (!parent_dir) {
            return NULL;
        }

        icon = g_file_resolve_relative_path(parent_dir, source);
        g_object_unref(parent_dir);
    }

    char* uri = g_file_get_uri(icon);
    g_object_unref(icon);
    return uri;
}

/**
 * @brief Appends a URI to a GString. Wraps the string in quotes and then escapes
 * special characters to w3 spec so that non-basic URIs don't break.
 * This also applies quotes so that you can pass something like
 * https://url.com
 * and get
 * "https://url.com"
 * As your response to pass directly to a css URL such as
 * background-image: url(YOUR-STRING);
 *
 * @param str The string to append the URI to.
 * @param value A raw URI path string.
 */
void append_quoted_uri(GString* str, const char* value)
{
    g_string_append_c(str, '"');

    // https://www.w3.org/TR/cssom-1/#serialize-a-string
    for (const unsigned char* c = (const unsigned char*)value; *c != '\0'; ++c) {
        switch (*c) {
            /** U+0022 = " */
            case '"':
                g_string_append(str, "\\\"");
                break;
            /** U+005C = \ */
            case '\\':
                g_string_append(str, "\\\\");
                break;
            /** NULL character means the end of string so we don't need to handle that */
            default:
                if ((*c >= 1 && *c <= 0x1F) || *c == 0x7F) {
                    g_string_append_printf(str, "\\%x ", *c);
                } else {
                    g_string_append_c(str, *c);
                }
        }
    }

    g_string_append_c(str, '"');
}

static void icon_input_free(gpointer data)
{
    IconInput* input = data;
    g_free(input->path);
    g_free(input->source);
    g_free(input->selector);
    g_free(input);
}

static void icon_request_free(IconRequest* request)
{
    if (request->input) {
        icon_input_free(request->input);
    }

    g_object_unref(request->provider);
    g_object_unref(request->cancellable);
    g_free(request);
}

static void icon_load(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    IconInput* input = task_data;
    if (g_task_return_error_if_cancelled(task)) {
        return;
    }

    // Even path resolution can block in GVfs, so it belongs in the worker.
    char* uri = icon_uri(input->path, input->source);
    if (!uri) {
        g_task_return_pointer(task, NULL, NULL);
        return;
    }

    if (g_ascii_strncasecmp(uri, "data:", 5) != 0) {
        GFile* file = g_file_new_for_uri(uri);
        GBytes* bytes = g_file_load_bytes(file, cancellable, NULL, NULL);
        g_object_unref(file);
        g_free(uri);
        uri = NULL;

        if (bytes) {
            gsize size;
            const guchar* data = g_bytes_get_data(bytes, &size);
            char* base64 = g_base64_encode(data, size);
            uri = g_strconcat("data:application/octet-stream;base64,", base64, NULL);
            g_free(base64);
            g_bytes_unref(bytes);
        }
    }

    if (g_task_return_error_if_cancelled(task)) {
        g_free(uri);
        return;
    }

    GString* css = g_string_new(input->selector);
    g_string_append(css, " { background-image: ");
    if (uri) {
        g_string_append(css, "url(");
        append_quoted_uri(css, uri);
        g_string_append_c(css, ')');
    } else {
        // If loading fails, just show nothing at least so the load doesn't fail.
        g_string_append(css, "none");
    }

    g_string_append(css, "; }");
    g_free(uri);

    char* result = g_string_free(css, FALSE);
    g_task_return_pointer(task, result, g_free);
}

static void icon_loaded(GObject* source_object, GAsyncResult* result, gpointer data);

static void icon_start_queued(void)
{
    while (icon_active < 4 && !g_queue_is_empty(&icon_queue)) {
        IconRequest* request = g_queue_pop_head(&icon_queue);
        GTask* task = g_task_new(NULL, request->cancellable, icon_loaded, request);
        g_task_set_task_data(task, request->input, icon_input_free);
        request->input = NULL;
        ++icon_active;
        g_task_run_in_thread(task, icon_load);
        g_object_unref(task);
    }
}

static void icon_loaded(GObject* source_object, GAsyncResult* result, gpointer data)
{
    IconRequest* request = data;
    char* css = g_task_propagate_pointer(G_TASK(result), NULL);
    if (css && !g_cancellable_is_cancelled(request->cancellable)) {
        gtk_css_provider_load_from_string(request->provider, css);
    }

    g_free(css);
    icon_request_free(request);

    --icon_active;
    icon_start_queued();
}

static GtkCssProvider* icon_provider_new(LSIcon* group)
{
    GtkCssProvider* provider = gtk_css_provider_new();
    g_ptr_array_add(group->providers, provider);
    gtk_style_context_add_provider_for_display(group->display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    return provider;
}

/**
 * @brief Creates an icon group and applies its base CSS.
 *
 * @param display The display to style.
 * @param base_css The base styles for the icon widgets.
 * @return The new group, to be released with ls_icon_free.
 */
LSIcon* ls_icon_new(GdkDisplay* display, const char* base_css)
{
    LSIcon* group = g_new0(LSIcon, 1);
    group->display = g_object_ref(display);
    group->cancellable = g_cancellable_new();
    group->providers = g_ptr_array_new_with_free_func(g_object_unref);
    gtk_css_provider_load_from_string(icon_provider_new(group), base_css);
    return group;
}

/**
 * @brief Queues an icon to load asynchronously and display through CSS.
 *
 * @param group The icon group.
 * @param path The split file path, used to resolve relative icon paths.
 * @param source The icon path, URL or data URL.
 * @param selector The CSS selector for the icon widgets.
 */
void ls_icon_add(LSIcon* group, const char* path, const char* source, const char* selector)
{
    IconRequest* request = g_new0(IconRequest, 1);
    // Reserve css order now, before any completion callbacks can run.
    request->provider = g_object_ref(icon_provider_new(group));
    request->cancellable = g_object_ref(group->cancellable);
    request->input = g_new0(IconInput, 1);
    request->input->path = g_strdup(path);
    request->input->source = g_strdup(source);
    request->input->selector = g_strdup(selector);
    g_queue_push_tail(&icon_queue, request);
    icon_start_queued();
}

/**
 * @brief Cancels icon loads, removes their CSS and frees the group.
 *
 * @param group The icon group to release, or NULL.
 */
void ls_icon_free(LSIcon* group)
{
    if (!group) {
        return;
    }

    g_cancellable_cancel(group->cancellable);
    for (GList* link = icon_queue.head; link;) {
        GList* next = link->next;
        IconRequest* request = link->data;
        if (request->cancellable == group->cancellable) {
            g_queue_delete_link(&icon_queue, link);
            icon_request_free(request);
        }

        link = next;
    }

    for (guint i = 0; i < group->providers->len; ++i) {
        gtk_style_context_remove_provider_for_display(group->display, GTK_STYLE_PROVIDER(g_ptr_array_index(group->providers, i)));
    }

    g_ptr_array_unref(group->providers);
    g_object_unref(group->cancellable);
    g_object_unref(group->display);
    g_free(group);
}
