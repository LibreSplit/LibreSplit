/** \file settings.c
 *
 * Implementation of the settings management
 */
#include "config.h"
#include <linux/limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <jansson.h>

#include "settings.h"

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
 * Checks and creates libresplit config directories.
 *
 * Performs a directory check, creating the libresplit
 * config directory if necessary.
 */
void check_directories()
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

/**
 * Gets the default config path, considering APPDIR if set.
 *
 * @param out_path A buffer to store the resulting path, which may be prepended with APPDIR if set for AppImages.
 */
void get_default_config_path(char* out_path)
{
    out_path[0] = '\0';
    if (getenv("APPDIR")) {
        strcat(out_path, getenv("APPDIR"));
    }
    strcat(out_path, DEFAULT_CONFIG_PATH);
}

/**
 * Copies the default JSON configuration file from DEFAULT_CONFIG_PATH
 * to another destination.
 *
 * @param dest_path The destination path to copy the config to.
 */
void copy_default_config(const char* dest_path)
{
    char default_config_path[PATH_MAX];
    get_default_config_path(default_config_path);
    FILE* src = fopen(default_config_path, "r");
    FILE* dest = fopen(dest_path, "w");

    if (!src || !dest) {
        perror("Failed to open source or destination config file for copying");
        // Only one of the two files might have failed to open
        if (src != NULL) {
            fclose(src);
        }
        if (dest != NULL) {
            fclose(dest);
        }
        return;
    }

    char ch;
    while ((ch = fgetc(src)) != EOF) {
        fputc(ch, dest);
    }

    fclose(src);
    fclose(dest);
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
 * Updates a setting, given section, key and new value. Automatically saves the user settings file.
 *
 * @param section The section name (usually "livesplit", "history" or "keybinds")
 * @param setting The setting name
 * @param value The new value to assign to the setting.
 */
void ls_update_setting(const char* section, const char* setting, json_t* value)
{
    char settings_path[PATH_MAX];
    get_libresplit_folder_path(settings_path);
    strcat(settings_path, "/settings.json");

    // Load existing settings
    json_t* root = NULL;
    FILE* file = fopen(settings_path, "r");
    if (file) {
        json_error_t error;
        root = json_loadf(file, 0, &error);
        fclose(file);
        if (!root) {
            printf("Failed to load settings: %s\n", error.text);
            return;
        }
    } else {
        // If file doesn't exist, create a new settings object
        root = json_object();
    }

    // Update specific setting
    json_t* ls_obj = json_object_get(root, section);
    if (!ls_obj) {
        ls_obj = json_object();
        json_object_set(root, section, ls_obj);
    }
    json_object_set_new(ls_obj, setting, value);

    // Save updated settings back to the file
    FILE* output_file = fopen(settings_path, "w");
    if (output_file) {
        json_dumpf(root, output_file, JSON_INDENT(4));
        fclose(output_file);
    } else {
        printf("Failed to save settings to %s\n", settings_path);
    }

    json_decref(root);
}

/**
 * Generic function that loads a json settings file into memory.
 *
 * @param settings_path The path to load the JSON file from.
 * @return The jansson JSON object containing the settings, or NULL if an error occurs.
 */
json_t* _load_from_json(const char* settings_path)
{
    FILE* file = fopen(settings_path, "r");
    if (file) {
        json_error_t error;
        json_t* root = json_loadf(file, 0, &error);
        fclose(file);
        if (!root) {
            printf("Failed to load settings from path %s: %s\n", settings_path, error.text);
            return NULL;
        }
        return root;
    } else {
        printf("Failed to open settings file: %s\n", settings_path);
        return NULL;
    }
}

/**
 * Function added for readability.
 *
 * Loads the default settings from the default configuration path.
 *
 * @return The jansson JSON object containing the default settings, or NULL if an error occurs.
 */
json_t* _load_default_settings()
{

    char default_config_path[PATH_MAX];
    get_default_config_path(default_config_path);
    return _load_from_json(default_config_path);
}

/**
 * Loads the user settings from LibreSplit's user config path.
 * If no file is available, the default settings will be copied into the user's config path.
 *
 * @return The jansson JSON object containing the user settings, or NULL if an error occurs.
 */
json_t* _load_user_settings()
{
    char settings_path[PATH_MAX];
    get_libresplit_folder_path(settings_path);
    strcat(settings_path, "/settings.json");

    struct stat st = { 0 };

    if (stat(settings_path, &st) == -1) {
        printf("Cannot find user settings file, copying default one\n");
        copy_default_config(settings_path);
    }

    return _load_from_json(settings_path);
}

/**
 * Loads LibreSplit's settings, eventually overlaying the user's settings on top of the default settings, allowing for seamless addition of new sections/keys/values.
 *
 * @return The jansson JSON object containing the settings, or NULL if an unrecoverable error occurs.
 */
json_t* load_settings()
{
    json_t* settings = _load_default_settings();
    json_t* user_settings = _load_user_settings();
    // If there are no user settings, load only the defaults
    if (user_settings == NULL) {
        return settings ? settings : NULL;
    }
    // If, for some reason, the defaults cannot be loaded, load the user settings only
    if (settings == NULL) {
        return user_settings ? user_settings : NULL;
    }
    // If both can be loaded, overlay settings with user_settings, to allow
    // for further updates down the line during development
    int merged = json_object_update_recursive(settings, user_settings);
    if (merged == 0) {
        // Merge successful, return the updated settings
        return settings;
    }
    printf("Failed to merge settings");
    // If the merge fails, return nothing
    return NULL;
}

/**
 * Loads a setting value from the settings file.
 *
 * @param section The section to get the setting from (usually "libresplit", "history" or "keybinds").
 * @param setting The key used search the setting.
 *
 * @return The jansson JSON object containing the setting value, or NULL if an error occurs or the setting is not found.
 */
json_t* get_setting_value(const char* section, const char* setting)
{
    json_t* root = load_settings();
    if (!root) {
        return NULL;
    }

    json_t* section_obj = json_object_get(root, section);
    if (!section_obj) {
        printf("Section '%s' not found\n", section);
        json_decref(root);
        return NULL;
    }

    json_t* value = json_object_get(section_obj, setting);
    if (!value) {
        printf("Setting '%s' not found in section '%s'\n", setting, section);
        json_decref(root);
        return NULL;
    }

    // Increment the reference count before returning
    json_incref(value);

    // Release the root object
    json_decref(root);

    return value;
}
