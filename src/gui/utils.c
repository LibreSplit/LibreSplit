#include <gtk/gtk.h>

static gpointer cancel_icon_load(gpointer data)
{
    g_usleep(G_USEC_PER_SEC);
    g_cancellable_cancel(data);
    g_object_unref(data);
    return NULL;
}

/**
 * @brief GTK's CSS image loader does not have a timeout. To prevent network hangs
 * fetch the resource ourselves first and then replace with embedded bytes to prevent
 * later network retries or network hangs from hanging the UI.
 *
 * @param file The resource file to load
 * @return char* The uri contents
 */
static char* remote_icon_uri(GFile* file)
{
    GCancellable* cancellable = g_cancellable_new();
    GThread* timeout = g_thread_new("icon-timeout", cancel_icon_load, g_object_ref(cancellable));
    GBytes* contents = g_file_load_bytes(file, cancellable, NULL, NULL);
    g_thread_unref(timeout);
    g_object_unref(cancellable);

    if (!contents) {
        return NULL;
    }

    gsize size;
    const guchar* bytes = g_bytes_get_data(contents, &size);
    char* encoded = g_base64_encode(bytes, size);
    char* uri = g_strconcat("data:application/octet-stream;base64,", encoded, NULL);
    g_free(encoded);
    g_bytes_unref(contents);
    return uri;
}

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
 * Remote icons are fetched with a one-second timeout and returned as embedded data, or NULL on failure.
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

    if (g_uri_peek_scheme(source)) {
        GFile* file = g_file_new_for_uri(source);
        gboolean local = g_file_is_native(file) || g_file_has_uri_scheme(file, "data") || g_file_has_uri_scheme(file, "resource");
        char* uri = local ? g_strdup(source) : remote_icon_uri(file);
        g_object_unref(file);
        return uri;
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
