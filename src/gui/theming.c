#include "theming.h"
#include "src/gui/app_window.h"
#include <linux/limits.h>
#include <string.h>
#include <sys/stat.h>

static inline const unsigned char* fallback_css_data(void)
{
    return _binary____src_fallback_css_start;
}
static inline size_t fallback_css_data_len(void)
{
    return (size_t)((uintptr_t)_binary____src_fallback_css_end - (uintptr_t)_binary____src_fallback_css_start);
}

static const char reset_rules[] = ".window.main-window{all:unset;}\n.window.main-window *{all:unset;}";

/**
 * Captures CSS loading errors the same way GTK 3's GError parameter did.
 *
 * @param provider The CSS provider that emitted the error (unused).
 * @param section The section containing the error.
 * @param error The parsing error or warning.
 * @param data The GError pointer that receives the first error.
 */
static void capture_css_error(GtkCssProvider* provider,
    GtkCssSection* section,
    const GError* error,
    gpointer data)
{
    GError** gerror = data;
    char* location;

    if (error->domain == GTK_CSS_PARSER_WARNING) {
        location = gtk_css_section_to_string(section);
        g_warning("Theme parsing error: %s: %s", location, error->message);
        g_free(location);
        return;
    }

    if (*gerror != NULL) {
        return;
    }

    *gerror = g_error_copy(error);
    if (section != NULL) {
        location = gtk_css_section_to_string(section);
        g_prefix_error(gerror, "%s", location);
        g_free(location);
    }
}

/**
 * Finds a theme, given its name and variant.
 *
 * @param win The LibreSplit Window.
 * @param name The name of the theme to load.
 * @param variant The name of the variant to load (can be empty).
 * @param out_path Pointer to a string onto which the theme path will be copied.
 *
 * @return 1 if the load is successful, 0 otherwise.
 */
int ls_app_window_find_theme(const LSAppWindow* win,
    const char* name,
    const char* variant,
    char* out_path)
{
    if (!name || !strlen(name)) {
        out_path[0] = '\0';
        return 0;
    }

    char theme_path[PATH_MAX];
    strcpy(theme_path, "/");
    strcat(theme_path, name);
    strcat(theme_path, "/");
    strcat(theme_path, name);
    if (variant && strlen(variant)) {
        strcat(theme_path, "-");
        strcat(theme_path, variant);
    }
    strcat(theme_path, ".css");

    strcpy(out_path, win->data_path);
    strcat(out_path, "/themes");
    strcat(out_path, theme_path);
    struct stat st = { 0 };
    if (stat(out_path, &st) == -1) {
        return 0;
    }
    return 1;
}

/**
 * Applies the reset rules to avoid Desktop Themes messing with LibreSplit
 *
 * @param win The LibreSplit window
 * @param gerror A GTK gerror pointer
 *
 * @return true if an error occurred
 */
static void apply_reset_rules(LSAppWindow* win, GError** gerror)
{
    gtk_style_context_add_provider_for_display(
        win->display,
        GTK_STYLE_PROVIDER(win->reset_style),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gulong error_handler = g_signal_connect(win->reset_style,
        "parsing-error", G_CALLBACK(capture_css_error), gerror);
    gtk_css_provider_load_from_string(GTK_CSS_PROVIDER(win->reset_style), reset_rules);
    g_signal_handler_disconnect(win->reset_style, error_handler);
}
/**
 * Loads a specific theme, with a fallback to the default theme
 *
 * @param win The LibreSplit window.
 * @param name The name of the theme to load.
 * @param variant The variant of the theme to load.
 */
void ls_app_load_theme_with_fallback(LSAppWindow* win, const char* name, const char* variant)
{
    char path[PATH_MAX];

    // Remove old style
    if (win->style) {
        gtk_style_context_remove_provider_for_display(
            win->display,
            GTK_STYLE_PROVIDER(win->style));
        g_object_unref(win->style);
        win->style = NULL;
    }

    GError* gerror = NULL;

    // If reset rules have never been loaded, create them
    if (!win->reset_style) {
        win->reset_style = gtk_css_provider_new();
        apply_reset_rules(win, &gerror);
        if (gerror != NULL) {
            g_printerr("Error loading theme reset Rules: %s\n", gerror->message);
            g_error_free(gerror);
            gerror = NULL;
        }
    }

    if (!win->style) {
        win->style = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            win->display,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    const bool found = ls_app_window_find_theme(win, name, variant, path);
    bool error = false;

    if (!found) {
        printf("Theme not found: \"%s\" (variant: \"%s\")\n", name ? name : "", variant ? variant : "");
    }

    if (found) {
        gulong error_handler = g_signal_connect(win->style, "parsing-error", G_CALLBACK(capture_css_error), &gerror);
        gtk_css_provider_load_from_path(
            GTK_CSS_PROVIDER(win->style),
            path);
        g_signal_handler_disconnect(win->style, error_handler);
        if (gerror != NULL) {
            g_printerr("Error loading custom theme CSS: %s\n", gerror->message);
            error = true;
            g_error_free(gerror);
            gerror = NULL;
        }
    }

    if (!found || error) {
        // Load default theme from embedded CSS as fallback
        GBytes* fallback_css = g_bytes_new_static(fallback_css_data(), fallback_css_data_len());
        gulong error_handler = g_signal_connect(win->style, "parsing-error", G_CALLBACK(capture_css_error), &gerror);
        gtk_css_provider_load_from_bytes(GTK_CSS_PROVIDER(win->style), fallback_css);
        g_signal_handler_disconnect(win->style, error_handler);
        g_bytes_unref(fallback_css);
        if (gerror != NULL) {
            g_printerr("Error loading default theme CSS: %s\n", gerror->message);
            g_error_free(gerror);
            gerror = NULL;
        }
    }
}
