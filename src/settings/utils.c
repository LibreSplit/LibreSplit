#include "utils.h"
#include "src/gui/widgets/alert.h"
#include "src/logging.h"
#include <linux/limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Creates a directory tree recursively.
 *
 * Works like the "mkdir -p" command on shell, creating
 * a directory and all its parents if necessary.
 *
 * Adapted from https://stackoverflow.com/a/2336245
 *
 * @param dir The path describing the resulting directory tree.
 * @param permissions The attributes used to create the directories.
 * @param name optional name for the error message on failure
 * @return bool Whether or not the directory creation was successful
 */
static bool mkdir_p(const char* dir, mode_t permissions, const char* name)
{
    char* path = NULL;
    char* p = NULL;
    size_t len;

    // create a mutable copy
    path = strdup(dir);
    if (path == NULL) {
        return false;
    }

    len = strlen(path);
    if (path[len - 1] == '/') {
        path[len - 1] = 0;
    }

    for (p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!create_default_directory(name != NULL ? name : "requested", path, permissions, NULL)) {
                free(path);
                return false;
            }

            *p = '/';
        }
    }

    bool ret = create_default_directory(name != NULL ? name : "requested", path, permissions, NULL);
    free(path);
    return ret;
}

/**
 * Copies the user's livesplit data path in a given string.
 *
 * @param out_path The string to copy the data path into.
 */
void get_libresplit_data_folder_path(char* out_path)
{
    struct passwd* pw = getpwuid(getuid());
    char* XDG_DATA_HOME = getenv("XDG_DATA_HOME");
    char* base_dir = strcat(pw->pw_dir, "/.local/share/libresplit");
    if (XDG_DATA_HOME != NULL) {
        char config_dir[PATH_MAX] = { 0 };
        strcpy(config_dir, XDG_DATA_HOME);
        strcat(config_dir, "/libresplit");
        strcpy(base_dir, config_dir);
    }
    strcpy(out_path, base_dir);
}

/**
 * Copies the user's livesplit configuration path in a given string.
 *
 * @param out_path The string to copy the configuration path into.
 */
void get_libresplit_folder_path(char* out_path)
{
    struct passwd* pw = getpwuid(getuid());
    char* XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
    char* base_dir = strcat(pw->pw_dir, "/.config/libresplit");
    if (XDG_CONFIG_HOME != NULL) {
        char config_dir[PATH_MAX] = { 0 };
        strcpy(config_dir, XDG_CONFIG_HOME);
        strcat(config_dir, "/libresplit");
        strcpy(base_dir, config_dir);
    }
    strcpy(out_path, base_dir);
}

/**
 * Checks and creates libresplit config directories.
 *
 * Performs a directory check, creating the libresplit
 * config directory if necessary.
 */
void check_directories(void)
{
    char libresplit_directory[PATH_MAX] = { 0 };
    get_libresplit_folder_path(libresplit_directory);

    char libresplit_data_directory[PATH_MAX] = { 0 };
    get_libresplit_data_folder_path(libresplit_data_directory);

    char auto_splitters_directory[PATH_MAX];
    char themes_directory[PATH_MAX];
    char splits_directory[PATH_MAX];
    char runs_directory[PATH_MAX];

    strcpy(auto_splitters_directory, libresplit_directory);
    strcat(auto_splitters_directory, "/auto-splitters");

    strcpy(themes_directory, libresplit_directory);
    strcat(themes_directory, "/themes");

    strcpy(splits_directory, libresplit_directory);
    strcat(splits_directory, "/splits");

    strcpy(runs_directory, libresplit_directory);
    strcat(runs_directory, "/runs");

    // Make the libresplit data directory if it doesn't exist
    if (!mkdir_p(libresplit_data_directory, 0755, "LibreSplit Data")) {
        // if this fails, it's unlikely that the config directory won't also fail
        // so return early so that we don't annoy the users with too many errors
        return;
    }

    // Make the libresplit config directory if it doesn't exist
    if (!mkdir_p(libresplit_directory, 0755, "LibreSplit Config")) {
        // if this fails the subdirectories below that depend on this will too
        return;
    }

    // Make the autosplitters directory if it doesn't exist
    create_default_directory("autosplitters directory", auto_splitters_directory, 0755, NULL);

    // Make the themes directory if it doesn't exist
    create_default_directory("themes directory", themes_directory, 0755, NULL);

    // Make the splits directory if it doesn't exist
    create_default_directory("splits directory", splits_directory, 0755, NULL);

    // Make the runs directory if it doesn't exist
    create_default_directory("runs directory", runs_directory, 0755, NULL);
}

/**
 * @brief Attempts to create a directory at a specified path, intended for the default directories LibreSplit uses.
 * If the directory already exist this returns true before creating. Otherwise the directory is attempted to be created.
 * If the creation fails, then displays an alert to the user indicating that the creation failed
 * and logs the error.
 *
 * @param name The name of the directory type i.e. Splits for the splits directory
 * @param path The path to the directory to create.
 * @param permissions The permissions to give to the directory.
 * @param parent The parent window for the potential user error message on failure.
 * @return bool Whether or not the directory creation was successful
 */
bool create_default_directory(const char* name, const char* path, mode_t permissions, GtkWindow* parent)
{
    struct stat st = { 0 };
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    if (mkdir(path, permissions) != 0) {
        LOG_ERRF("Failed to create directory: %s: %s", path, strerror(errno));

        char error_msg[PATH_MAX];
        snprintf(error_msg, sizeof error_msg, "We were unable to create the %s directory at:\n%s", name, path);
        ls_alert_error(parent, "Error", "Unable to create directory", error_msg);
        return false;
    }

    return true;
}
