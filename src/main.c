#include "gui/app_window.h"
#include "gui/dialogs.h"
#include "gui/timer.h"
#include "lasr/auto-splitter.h"
#include "logging.h"
#include "server.h"
#include "settings/utils.h"
#include "shared.h"
#include "src/gui/dialogs.h"

#include <gtk/gtk.h>
#include <jansson.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>

atomic_bool exit_requested = 0; /*!< Set to 1 when LibreSplit is exiting */

// Global application instance for CTL command handling
static LSApp* g_app = NULL;

// Function to handle CTL commands from the server thread
void handle_ctl_command(CTLCommand command)
{
    if (!g_app) {
        LOG_INFO("No application instance available to handle commands");
        return;
    }

    LSAppWindow* win = ls_get_main_app_window(GTK_APPLICATION(g_app));
    if (!win) {
        LOG_INFO("No window available to handle commands");
        return;
    }

    switch (command) {
        case CTL_CMD_START_SPLIT:
            LOG_DEBUG("Split requested via Server Command");
            timer_start_split(win);
            break;
        case CTL_CMD_STOP_RESET:
            LOG_DEBUG("Run Stop/Reset requested via Server Command");
            timer_stop_or_reset(win);
            break;
        case CTL_CMD_CANCEL:
            LOG_DEBUG("Run Cancellation requested via Server Command");
            timer_cancel_run(win);
            break;
        case CTL_CMD_UNSPLIT:
            LOG_DEBUG("Unsplit requested via Server Command");
            timer_unsplit(win);
            break;
        case CTL_CMD_SKIP:
            LOG_DEBUG("Skip requested via Server Command");
            timer_skip(win);
            break;
        case CTL_CMD_EXIT:
            LOG_DEBUG("Exit requested via Server Command");
            gtk_window_close(GTK_WINDOW(win));
            break;
        default:
            LOG_INFOF("Unknown CTL command: %d", command);
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
        if (atomic_load(&exit_requested)) {
            LOG_DEBUG("Exit requested, shutting down Auto Splitter Thread");
            return 0;
        }
        usleep(50000);
    }
    return NULL;
}

static bool bypass_root_protection(void)
{
    const char* env = getenv("BYPASS_ROOT_PROTECTION_CHECKS");
    return env != NULL && strcmp(env, "1") == 0;
}

int main(int argc, char* argv[])
{
    // Check if app is running as root.
    if (geteuid() == 0 && !bypass_root_protection()) {
        gtk_init();
        display_root_warning_dialog();
        return 1;
    }

    initLogQueue();
    LOG_INFOF("Starting LibreSplit - version %s", APP_VERSION);
    check_directories();

    g_app = ls_app_new();
    LOG_INFO("Creating Auto-Splitter Thread");
    pthread_t t1; // Auto-splitter thread
    pthread_create(&t1, NULL, &ls_auto_splitter, NULL);

    LOG_INFO("Creating Control Server Thread");
    pthread_t t2; // Control server thread
    pthread_create(&t2, NULL, &ls_ctl_server, NULL);

    LOG_INFO("Creating Log Consumer Thread");
    pthread_t t3; // Logging Thread
    pthread_create(&t3, NULL, &loggingThread, NULL);

    int status = g_application_run(G_APPLICATION(g_app), argc, argv);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    g_clear_object(&g_app);
    return status;
}
