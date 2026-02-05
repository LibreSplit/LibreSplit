/**
 * Shows a message dialog in case of a memory read error.
 *
 * @param data Unused.
 *
 * @return False, to remove the function from the queue.
 */
#include "src/lasr/auto-splitter.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <stdatomic.h>
#include <stdbool.h>

/**
 * Opens the default browser on the LibreSplit troubleshooting documentation.
 *
 * @param dialog The dialog that triggered this callback.
 * @param response_id Unused.
 * @param user_data Unused.
 */
static void dialog_response_cb(GtkWidget* dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_OK) {
        gtk_show_uri_on_window(GTK_WINDOW(NULL), "https://github.com/LibreSplit/LibreSplit/wiki/troubleshooting", 0, NULL);
    }
    gtk_widget_destroy(dialog);
}

gboolean display_non_capable_mem_read_dialog(gpointer data)
{
    atomic_store(&auto_splitter_enabled, 0);
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(NULL),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_NONE,
        "LibreSplit was unable to read memory from the target process.\n"
        "This is most probably due to insufficient permissions.\n"
        "This only happens on linux native games/binaries.\n"
        "Try running the game/program via steam.\n"
        "Autosplitter has been disabled.\n"
        "This warning will only show once until libresplit restarts.\n"
        "Please read the troubleshooting documentation to solve this error without running as root if the above doesnt work\n"
        "");

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
        "Close", GTK_RESPONSE_CANCEL,
        "Open documentation", GTK_RESPONSE_OK, NULL);

    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response_cb), NULL);
    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    // Connect the response signal to the callback function
    return FALSE; // False removes this function from the queue
}

bool display_confirm_reset_dialog()
{
    GtkWidget* dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "This run contains a gold split.\n\n"
        "Are you sure you want to reset?");
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Reset?");

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return response == GTK_RESPONSE_YES;
}

/**
 * Shows a warning dialog when user tries to change theme but a split has its own theme.
 *
 * @param parent_window The parent window for the dialog.
 * @param split_theme_name The theme name from the current split.
 * @param split_theme_variant The theme variant from the current split (can be NULL).
 */
void show_split_theme_warning(GtkWindow* parent_window, const char* split_theme_name, const char* split_theme_variant)
{
    char message[1024];
    if (split_theme_variant && strlen(split_theme_variant) > 0) {
        snprintf(message, sizeof(message),
            "Cannot change theme settings.\n\n"
            "The current split has its own theme: \"%s\" (variant: \"%s\")\n\n"
            "To change the global theme, either:\n"
            "• Close the current split, or\n"
            "• Load a split without a preset theme",
            split_theme_name, split_theme_variant);
    } else {
        snprintf(message, sizeof(message),
            "Cannot change theme settings.\n\n"
            "The current split has its own theme: \"%s\"\n\n"
            "To change the global theme, either:\n"
            "• Close the current split, or\n"
            "• Load a split without a preset theme",
            split_theme_name);
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK,
        "%s", message);

    gtk_window_set_title(GTK_WINDOW(dialog), "Theme Override Active");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/**
 * Shows an error dialog for invalid theme names.
 *
 * @param parent_window The parent window for the dialog.
 * @param theme_name The invalid theme name.
 * @param theme_variant The invalid theme variant (can be NULL).
 */
void show_theme_error_dialog(GtkWindow* parent_window, const char* theme_name, const char* theme_variant)
{
    // Calculate needed buffer size
    size_t needed_size = 500; // Base message size
    needed_size += strlen(theme_name) * 3; // theme_name appears 3 times
    if (theme_variant && strlen(theme_variant) > 0) {
        needed_size += strlen(theme_variant) * 2; // variant appears 2 times
        needed_size += 100; // extra text for variant case
    }

    char* message = malloc(needed_size);
    if (!message) {
        // Fallback to simple error if malloc fails
        GtkWidget* dialog = gtk_message_dialog_new(
            parent_window,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Theme not found. Please check your theme configuration.");
        gtk_window_set_title(GTK_WINDOW(dialog), "Theme Not Found");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    if (theme_variant && strlen(theme_variant) > 0) {
        snprintf(message, needed_size,
            "Theme not found: \"%s\" (variant: \"%s\")\n\n"
            "Please check the theme name and variant are correct.\n"
            "The theme files should be located in:\n"
            "$XDG_CONFIG_HOME/libresplit/themes/%s/%s-%s.css",
            theme_name, theme_variant, theme_name, theme_name, theme_variant);
    } else {
        snprintf(message, needed_size,
            "Theme not found: \"%s\"\n\n"
            "Please check the theme name is correct.\n"
            "The theme file should be located in:\n"
            "$XDG_CONFIG_HOME/libresplit/themes/%s/%s.css",
            theme_name, theme_name, theme_name);
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s", message);

    gtk_window_set_title(GTK_WINDOW(dialog), "Theme Unavaliable");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    free(message);
}
