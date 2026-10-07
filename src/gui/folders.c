#include "gui/folders.h"
#include "logging.h"
#include "settings/utils.h"
#include <glib.h>
#include <stdio.h>

typedef enum DirectoryType {
    Config,
    Data,
} DirectoryType;

typedef struct DirectoryTypeMap {
    const char* dir;
    enum DirectoryType type;
} DirectoryTypeMap;

/**
 * @brief A map of default directories to the default path type
 */
static const struct DirectoryTypeMap type_map[] = {
    { FOLDERS_SPLITS_DIR, Config },
    { FOLDERS_AUTO_SPLITTERS_DIR, Config },
    { FOLDERS_THEMES_DIR, Config },
    { FOLDERS_LOGS_DIR, Data },
};

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
 * @brief Get the default dir path for type of directory requested.
 *
 * @param dir The requested directory, must be one of the defined dirs in folders.h.
 * @param path The string to store the directory path for.
 * @return bool Whether or not the directory was written to path.
 */
static bool get_dir_path_for_type(const char* dir, char* path)
{
    if (dir == NULL || path == NULL) {
        LOG_WARN("[Open Folder] Invalid usage dir or path are null");
        return false;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(type_map); ++i) {
        if (g_str_equal(type_map[i].dir, dir)) {
            bool res = false;
            switch (type_map[i].type) {
                case Config:
                    get_libresplit_folder_path(path);
                    res = true;
                    break;

                case Data:
                    get_libresplit_data_folder_path(path);
                    res = true;
                    break;
            }

            if (res) {
                size_t used = strnlen(path, PATH_MAX);
                if (used == 0 || used == PATH_MAX) {
                    LOG_ERRF("[Open Folder] Can't create %s path", type_map[i].dir);
                    return false;
                }

                size_t remaining = PATH_MAX - used;
                int written = snprintf(path + used, remaining, "/%s", type_map[i].dir);
                if (written < 0 || (size_t)written >= remaining) {
                    if (written < 0) {
                        LOG_ERRF("[Open Folder] Cannot create %s path", type_map[i].dir);
                    } else {
                        LOG_ERRF("[Open Folder] Folder path for %s is too long", type_map[i].dir);
                    }

                    path[0] = '\0';
                    return false;
                }
            }

            return res;
        }
    }

    return false;
}

/**
 * Launches the default file manager for the specified directory
 *
 * @param action Unused
 * @param parameter The directory specified by the action
 * @param app Unused
 */
void launch_fm_dir(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    char path[PATH_MAX];
    const char* dir = g_variant_get_string(parameter, NULL);
    LOG_INFOF("Opening the default File Manager for the %s folder", dir);
    if (!get_dir_path_for_type(dir, path)) {
        return;
    }

    GError* error = NULL;
    gboolean result = launch_file_manager(path, &error);
    if (!result) {
        LOG_ERRF("[Open Folder] Error while opening folder: %s", error->message);
        g_error_free(error);
    }
}
