/** \file settings.c
 *
 * Implementation of the settings management
 */
#include "settings.h"

#include "definitions.h"
#include "utils.h"

#include <linux/limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <jansson.h>

#include <stdbool.h>

/**
 * Populate a ConfigEntry from a JSON value.
 * @e: Pointer to the ConfigEntry to update.
 * @v: JSON value to read from.
 *
 * Validates that the JSON value matches the entry type and copies the
 * value into the ConfigEntry. Returns true on success, false on NULL
 * value or type mismatch.
 */
static bool set_entry_from_json(ConfigEntry* e, json_t* v)
{
    if (!v)
        return false;
    switch (e->type) {
        case CFG_BOOL:
            if (!json_is_boolean(v))
                return false;
            e->value.b = json_is_true(v);
            return true;
        case CFG_INT:
            if (!json_is_integer(v))
                return false;
            e->value.i = (int)json_integer_value(v);
            return true;
        case CFG_STRING:
            if (!json_is_string(v))
                return false;
            strncpy(e->value.s, json_string_value(v), sizeof(e->value.s) - 1);
            return true;
    }
    return false;
}

/**
 * Convert a ConfigEntry into a JSON value.
 * @e: Pointer to the ConfigEntry to convert.
 *
 * Returns a new json_t reference representing the entry's value, or
 * json_null() for unknown types. Caller is responsible for managing
 * the returned reference
 */
static json_t* json_from_entry(const ConfigEntry* e)
{
    switch (e->type) {
        case CFG_BOOL:
            return json_pack("b", e->value.b);
        case CFG_INT:
            return json_pack("i", e->value.i);
        case CFG_STRING:
            return json_pack("s", e->value.s);
        default:
            printf("Unknown config entry type \"%d\". Returning null JSON field.\n", e->type);
            break;
    }
    return json_null();
}

/**
 * Loads settings.json and merge values with defaults.
 *
 * Loads JSON into `cfg` merging with defaults: defaults are already set,
 * then any present and correctly-typed values in the JSON override them.
 *
 * Returns true on success (including when the file is missing or not
 * a valid JSON object since it will return defaults), false on unrecoverable errors.
 */
bool config_init(void)
{
    char path[PATH_MAX] = { 0 };
    get_libresplit_folder_path(path);
    strcat(path, "/settings.json");

    // cfg is already initialized with defaults
    // so we just have to overwrite them

    json_error_t err;
    json_t* root = json_load_file(path, 0, &err);
    if (!root) {
        /* Could not load JSON -> keep defaults */
        return true;
    }

    if (!json_is_object(root)) {
        json_decref(root);
        return true;
    }

    for (size_t s = 0; s < sections_count; ++s) {
        SectionInfo* sec = &sections[s];
        json_t* sec_obj = json_object_get(root, sec->name);
        if (!sec_obj || !json_is_object(sec_obj))
            continue; // leave defaults for this section

        for (size_t i = 0; i < sec->count; ++i) {
            ConfigEntry* e = (ConfigEntry*)((char*)sec->entries + (sizeof(ConfigEntry) * i));
            json_t* v = json_object_get(sec_obj, e->key);
            if (!set_entry_from_json(e, v)) {
                // invalid or missing, keep default
            }
        }
    }

    json_decref(root);
    return true;
}

/**
 * Serialize the current configuration to settings.json.
 *
 * Builds a JSON object from all config sections and entries and writes
 * it to the LibreSplit folder's settings.json with indentation and
 * preserved order.
 *
 * Returns true on successful write, false otherwise.
 */
bool config_save(void)
{
    json_t* root = json_object();
    if (!root)
        return false;

    for (size_t s = 0; s < sections_count; ++s) {
        SectionInfo* sec = &sections[s];
        json_t* sec_obj = json_object();
        for (size_t i = 0; i < sec->count; ++i) {
            ConfigEntry* e = (ConfigEntry*)((char*)sec->entries + (sizeof(ConfigEntry) * i));
            json_t* v = json_from_entry(e);
            if (v && !json_is_null(v))
                json_object_set_new(sec_obj, e->key, v);
            else {
                printf("Cannot save field \"%s\" with type \"%d\"\n", e->key, e->type);
                if (v)
                    json_decref(v);
            }
        }
        json_object_set_new(root, sec->name, sec_obj);
    }

    char path[PATH_MAX] = { 0 };
    get_libresplit_folder_path(path);
    strcat(path, "/settings.json");

    check_directories();
    int ret = json_dump_file(root, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);
    return ret == 0;
}
