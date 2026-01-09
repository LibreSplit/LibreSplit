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
 * Taken from https://stackoverflow.com/a/2336245
 *
 * @param dir The path describing the resulting directory tree.
 * @param permissions The attributes used to create the directories.
 */
// I have no idea how this works
static void mkdir_p(const char* dir, __mode_t permissions)
{
    char tmp[256] = { 0 };
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, permissions);
            *p = '/';
        }
    mkdir(tmp, permissions);
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

    char auto_splitters_directory[PATH_MAX];
    char themes_directory[PATH_MAX];
    char splits_directory[PATH_MAX];

    strcpy(auto_splitters_directory, libresplit_directory);
    strcat(auto_splitters_directory, "/auto-splitters");

    strcpy(themes_directory, libresplit_directory);
    strcat(themes_directory, "/themes");

    strcpy(splits_directory, libresplit_directory);
    strcat(splits_directory, "/splits");

    // Make the libresplit directory if it doesn't exist
    mkdir_p(libresplit_directory, 0755);

    // Make the autosplitters directory if it doesn't exist
    if (mkdir(auto_splitters_directory, 0755) == -1) {
        // Directory already exists or there was an error
    }

    // Make the themes directory if it doesn't exist
    if (mkdir(themes_directory, 0755) == -1) {
        // Directory already exists or there was an error
    }

    // Make the splits directory if it doesn't exist
    if (mkdir(splits_directory, 0755) == -1) {
        // Directory already exists or there was an error
    }
}
