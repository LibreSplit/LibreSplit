#include "src/gui/dialogs.h"
#include "src/gui/widgets/dialog.h"
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

static void open_troubleshoot_page_finished(GObject* launcher_ref, GAsyncResult* result, gpointer user_data)
{
    GtkUriLauncher* launcher = GTK_URI_LAUNCHER(launcher_ref);
    GError* error = NULL;
    if (!gtk_uri_launcher_launch_finish(launcher, result, &error)) {
        g_warning("Could not open URI: %s", error->message);
        g_error_free(error);
    }

    g_object_unref(launcher);
}

/**
 * Opens the default browser on the LibreSplit troubleshooting documentation.
 *
 * @param user_data The parent window.
 */
static void open_troubleshoot_page(gpointer user_data)
{
    GtkWindow* parent = GTK_WINDOW(user_data);
    GtkUriLauncher* launcher = gtk_uri_launcher_new("https://github.com/LibreSplit/LibreSplit/wiki/troubleshooting");
    gtk_uri_launcher_launch(launcher, parent, NULL, open_troubleshoot_page_finished, NULL);
}

/**
 * Shows a message dialog in case of a memory read error.
 */
void display_non_capable_mem_read_dialog()
{
    atomic_store(&auto_splitter_enabled, 0);
    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    GtkWindow* win = NULL;
    if (app != NULL) {
        win = gtk_application_get_active_window(app);
    }

    const LSDialogOption options[] = {
        {
            .label = "_Close",
            .callback = NULL,
            .is_cancel = TRUE,
            .is_default = FALSE,
        },
        {
            .label = "_Open documentation",
            .callback = open_troubleshoot_page,
            .is_cancel = FALSE,
            .is_default = TRUE,
        }
    };

    const LSDialogIcon icon = {
        .source = "dialog-error",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_dialog_open(
        win != NULL ? GTK_WINDOW(win) : NULL,
        "Memory Read Error",
        "LibreSplit was unable to read memory from the target process",
        "This is most probably due to insufficient permissions.\n"
        "This only happens on linux native games/binaries.\n"
        "Try running the game/program via steam.\n"
        "Autosplitter has been disabled.\n"
        "This warning will only show once until libresplit restarts.\n"
        "Please read the troubleshooting documentation to solve this error without running as root if the above doesnt work",
        &icon,
        options,
        G_N_ELEMENTS(options),
        win,
        NULL);
}

/**
 * @brief Quit the wait loop for the root warning alert
 *
 * @param data the loop
 */
static void root_warning_finish(gpointer data)
{
    GMainLoop* loop = data;
    g_main_loop_quit(loop);
    g_main_loop_unref(loop);
}

/**
 * Displays a modal warning dialog explaining that LibreSplit should not be
 * run as the root user due to potential security and file permission issues.
 * The dialog is parented to the active application window when one exists.
 *
 * @return this is called in main and returns this function's value so return 1 to indicate error
 */
int display_root_warning_dialog()
{
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    const LSDialogOption options[] = {
        {
            .label = "_OK",
            .callback = NULL,
            .is_cancel = FALSE,
            .is_default = TRUE,
        }
    };

    const LSDialogIcon icon = {
        .source = "dialog-warning",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_dialog_open(
        NULL,
        "Unsafe Configuration",
        "Running LibreSplit as root is unsafe",
        "Running applications as root can lead to security issues\n"
        "and may cause unintended file ownership problems.\n"
        "Please run LibreSplit as a normal user.",
        &icon,
        options,
        G_N_ELEMENTS(options),
        loop,
        root_warning_finish);

    g_main_loop_run(loop);
    return 1;
}

bool display_confirm_reset_dialog()
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
