#include "theming.h"
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
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(win->style));
        g_object_unref(win->style);
        win->style = nullptr;
    }

    if (!win->style) {
        win->style = gtk_css_provider_new();
    }

    GError* gerror = nullptr;

    const bool found = ls_app_window_find_theme(win, name, variant, path);
    bool error = false;

    if (!found) {
        printf("Theme not found: \"%s\" (variant: \"%s\")\n", name ? name : "", variant ? variant : "");
    }

    if (found) {
        GdkScreen* screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(
            GTK_CSS_PROVIDER(win->style),
            path, &gerror);
        if (gerror != nullptr) {
            g_printerr("Error loading custom theme CSS: %s\n", gerror->message);
            error = true;
            g_error_free(gerror);
            gerror = nullptr;
        }
    }

    if (!found || error) {
        // Load default theme from embedded CSS as fallback
        GdkScreen* screen = gdk_display_get_default_screen(win->display);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(win->style),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_data(
            GTK_CSS_PROVIDER(win->style),
            (const char*)fallback_css_data(),
            (gssize)fallback_css_data_len(), &gerror);
        if (gerror != nullptr) {
            g_printerr("Error loading default theme CSS: %s\n", gerror->message);
            error = true;
            g_error_free(gerror);
            gerror = nullptr;
        }
    }
}
