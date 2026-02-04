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

bool display_confirm_reset_dialog(void)
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
