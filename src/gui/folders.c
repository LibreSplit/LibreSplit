#include "folders.h"
#include "src/logging.h"
#include "src/settings/utils.h"
#include <glib.h>
#include <stdio.h>

/**
 * Launches the default file manager for a certain path.
 *
 * @param path The path to open
 * @param error The GError instance to fill in case of errors
 *
 * @return Whether the FM launch is successful.
 */
static gboolean launch_file_manager(const char* path, GError** error)
{
    GFile* folder = g_file_new_for_path(path);
    char* uri = g_file_get_uri(folder);
    gboolean result = g_app_info_launch_default_for_uri(uri, NULL, error);
    g_free(uri);
    g_object_unref(folder);
    return result;
}

/**
 * Launches the default file manager for the splits folder
 *
 * @param action Unused
 * @param parameter unused
 * @param app Unused
 */
void launch_fm_splits(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    LOG_INFO("Opening the default File Manager for the splits folder");
    char libresplit_path[PATH_MAX];
    get_libresplit_folder_path(libresplit_path);
    char path[PATH_MAX];
    int written = snprintf(path, PATH_MAX, "%s/splits", libresplit_path);
    if (written < 0 || written >= PATH_MAX) {
        if (written < 0) {
            LOG_ERR("[Open Folder] Cannot create splits path");
        } else {
            LOG_ERR("[Open Folder] Splits folder path is too long");
        }
        path[0] = '\0';
        return;
    }
    GError* error;
    gboolean result = launch_file_manager(path, &error);
    if (!result) {
        LOG_ERRF("[Open Folder] Error while opening folder: %s", error->message);
    }
    return;
}

/**
 * Launches the default file manager for the Auto Splitters folder
 *
 * @param action Unused
 * @param parameter unused
 * @param app Unused
 */
void launch_fm_autosplitters(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    LOG_INFO("Opening the default File Manager for the auto splitters folder");
    char libresplit_path[PATH_MAX];
    get_libresplit_folder_path(libresplit_path);
    char path[PATH_MAX];
    int written = snprintf(path, PATH_MAX, "%s/auto-splitters", libresplit_path);
    if (written < 0 || written >= PATH_MAX) {
        if (written < 0) {
            LOG_ERR("[Open Folder] Cannot create auto splitters path");
        } else {
            LOG_ERR("[Open Folder] Auto Splitters folder path is too long");
        }
        path[0] = '\0';
        return;
    }
    GError* error;
    gboolean result = launch_file_manager(path, &error);
    if (!result) {
        LOG_ERRF("[Open Folder] Error while opening folder: %s", error->message);
    }
    return;
}

/**
 * Launches the default file manager for the Themes folder
 *
 * @param action Unused
 * @param parameter unused
 * @param app Unused
 */
void launch_fm_themes(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    LOG_INFO("Opening the default File Manager for the themes folder");
    char libresplit_path[PATH_MAX];
    get_libresplit_folder_path(libresplit_path);
    char path[PATH_MAX];
    int written = snprintf(path, PATH_MAX, "%s/themes", libresplit_path);
    if (written < 0 || written >= PATH_MAX) {
        if (written < 0) {
            LOG_ERR("[Open Folder] Cannot create themes path");
        } else {
            LOG_ERR("[Open Folder] Themes folder path is too long");
        }
        path[0] = '\0';
        return;
    }
    GError* error;
    gboolean result = launch_file_manager(path, &error);
    if (!result) {
        LOG_ERRF("[Open Folder] Error while opening folder: %s", error->message);
    }
    return;
}

/**
 * Launches the default file manager for the logs folder
 *
 * @param action Unused
 * @param parameter unused
 * @param app Unused
 */
void launch_fm_logs(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    LOG_INFO("Opening the default File Manager for the logs folder");
    char libresplit_path[PATH_MAX];
    get_libresplit_data_folder_path(libresplit_path);
    // TODO: [Penaz] [2026-09-23] Keeping this useless snprintf because
    // ^ I want logs to be in a subfolder (and have them rotate) in the near future.
    char path[PATH_MAX];
    int written = snprintf(path, PATH_MAX, "%s", libresplit_path);
    if (written < 0 || written >= PATH_MAX) {
        if (written < 0) {
            LOG_ERR("[Open Folder] Cannot create logs path");
        } else {
            LOG_ERR("[Open Folder] Logs folder path is too long");
        }
        path[0] = '\0';
        return;
    }
    GError* error;
    gboolean result = launch_file_manager(path, &error);
    if (!result) {
        LOG_ERRF("[Open Folder] Error while opening folder: %s", error->message);
    }
    return;
}
