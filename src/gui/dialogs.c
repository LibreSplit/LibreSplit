#include "src/gui/dialogs.h"
#include "src/lasr/auto-splitter.h"
#include "src/logging.h"
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdatomic.h>
#include <stdbool.h>

// TODO: Remove temp helpers
/** Temporary sync dialog compatibility helpers - ignore these */
typedef struct {
    GMainLoop* loop;
    gint response;
} DialogRun;

static void dialog_response(GtkDialog* dialog, gint response, gpointer data)
{
    DialogRun* run = data;
    run->response = response;
    g_main_loop_quit(run->loop);
}

static gboolean dialog_close_request(GtkWindow* window, gpointer data)
{
    DialogRun* run = data;
    run->response = GTK_RESPONSE_DELETE_EVENT;
    g_main_loop_quit(run->loop);
    return TRUE;
}

gint run_dialog(GtkDialog* dialog)
{
    DialogRun run = {
        .loop = g_main_loop_new(NULL, FALSE),
        .response = GTK_RESPONSE_NONE,
    };
    gboolean was_modal = gtk_window_get_modal(GTK_WINDOW(dialog));
    gulong response_handler = g_signal_connect(dialog,
        "response", G_CALLBACK(dialog_response), &run);
    gulong close_handler = g_signal_connect(dialog,
        "close-request", G_CALLBACK(dialog_close_request), &run);

    if (!was_modal) {
        gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    }
    gtk_window_present(GTK_WINDOW(dialog));
    g_main_loop_run(run.loop);

    if (!was_modal) {
        gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
    }
    g_signal_handler_disconnect(dialog, response_handler);
    g_signal_handler_disconnect(dialog, close_handler);
    g_main_loop_unref(run.loop);

    return run.response;
}
/** End temporary sync dialog compatibility helpers */

/**
 * Opens the default browser on the LibreSplit troubleshooting documentation.
 *
 * @param dialog The dialog that triggered this callback.
 * @param response_id The selected dialog response.
 * @param user_data Unused.
 */
static void dialog_response_cb(GtkDialog* dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_OK) {
        gtk_show_uri(NULL, "https://github.com/LibreSplit/LibreSplit/wiki/troubleshooting", 0);
    }
}

/**
 * Shows a message dialog in case of a memory read error.
 *
 * @param data Unused.
 *
 * @return False, to remove the function from the queue.
 */
gboolean display_non_capable_mem_read_dialog(gpointer data)
{
    atomic_store(&auto_splitter_enabled, 0);
    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    GtkWindow* win = NULL;
    if (app != NULL) {
        win = gtk_application_get_active_window(app);
    }
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(win),
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
    run_dialog(GTK_DIALOG(dialog));
    gtk_window_destroy(GTK_WINDOW(dialog));

    // Connect the response signal to the callback function
    return FALSE; // False removes this function from the queue
}

/**
 * Displays a modal warning dialog explaining that LibreSplit should not be
 * run as the root user due to potential security and file permission issues.
 * The dialog is parented to the active application window when one exists.
 *
 * @return `true` to indicate that root execution was detected and the warning
 *         dialog was shown.
 */
bool display_root_warning_dialog(void)
{
    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    GtkWindow* win = NULL;

    if (app != NULL) {
        win = gtk_application_get_active_window(app);
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(win),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK,
        "Running LibreSplit as root is unsafe.\n\n"
        "Running applications as root can lead to security issues "
        "and may cause unintended file ownership problems.\n\n"
        "Please run LibreSplit as a normal user.");

    gtk_window_set_title(GTK_WINDOW(dialog), "Unsafe Configuration");

    run_dialog(GTK_DIALOG(dialog));
    gtk_window_destroy(GTK_WINDOW(dialog));

    return true;
}

/**
 * Displays a dialog asking for confirmation for a reset when
 * there is a gold split involved.
 *
 * @return True or false, depending on whether on how the user answered the dialog
 */
bool display_confirm_reset_dialog(void)
{
    LOG_DEBUG("Detected gold/rainbow split, asking user for confirmation");
    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    GtkWindow* win = NULL;
    if (app != NULL) {
        win = gtk_application_get_active_window(app);
    }
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(win),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "This run contains a gold and/or rainbow split.\n\n"
        "Are you sure you want to proceed?");
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Reset?");

    gint response = run_dialog(GTK_DIALOG(dialog));
    gtk_window_destroy(GTK_WINDOW(dialog));
    return response == GTK_RESPONSE_YES;
}
