#include "gui/app_window.h"
#include "gui/timer.h"
#include "keybinds/keybinds_callbacks.h"
#include "lasr/auto-splitter.h"
#include "server.h"
#include "settings/utils.h"
#include "shared.h"
#include "src/keybinds/delayed_callbacks.h"

#include <gtk/gtk.h>
#include <jansson.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>

atomic_bool exit_requested = 0; /*!< Set to 1 when LibreSplit is exiting */

// Global application instance for CTL command handling
static LSApp* g_app = NULL;

// Function to handle CTL commands from the server thread
void handle_ctl_command(CTLCommand command)
{
    GList* windows;
    LSAppWindow* win;

    if (!g_app) {
        printf("No application instance available to handle command\n");
        return;
    }

    windows = gtk_application_get_windows(GTK_APPLICATION(g_app));
    if (windows) {
        win = LS_APP_WINDOW(windows->data);
    } else {
        printf("No window available to handle command\n");
        return;
    }

    switch (command) {
        case CTL_CMD_START_SPLIT:
            timer_start_split(win);
            break;
        case CTL_CMD_STOP_RESET:
            timer_stop_or_reset(win);
            break;
        case CTL_CMD_CANCEL:
            timer_cancel_run(win);
            break;
        case CTL_CMD_UNSPLIT:
            timer_unsplit(win);
            break;
        case CTL_CMD_SKIP:
            timer_skip(win);
            break;
        case CTL_CMD_EXIT:
            exit(0);
            break;
        default:
            printf("Unknown CTL command: %d\n", command);
            break;
    }
}

/**
 * LibreSplit's auto splitter thread.
 *
 * @param arg Unused.
 */
static void* ls_auto_splitter(void* arg)
{
    prctl(PR_SET_NAME, "LS LASR", 0, 0, 0);
    while (1) {
        if (atomic_load(&auto_splitter_enabled) && auto_splitter_file[0] != '\0') {
            atomic_store(&auto_splitter_running, true);
            run_auto_splitter();
        }
        atomic_store(&auto_splitter_running, false);
        if (atomic_load(&exit_requested))
            return 0;
        usleep(50000);
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    check_directories();

    g_app = ls_app_new();
    pthread_t t1; // Auto-splitter thread
    pthread_create(&t1, NULL, &ls_auto_splitter, NULL);

    pthread_t t2; // Control server thread
    pthread_create(&t2, NULL, &ls_ctl_server, NULL);

    g_application_run(G_APPLICATION(g_app), argc, argv);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
