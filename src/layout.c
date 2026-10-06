#include "layout.h"

#include "gui/component/components.h"
#include "logging.h"
#include "settings/utils.h"
#include <sys/stat.h>

#define VALIDATION_PREFIX "The loaded layout was incorrectly formatted: "

static bool validate_layout(json_t* root)
{
    if (!json_is_array(root)) {
        LOG_ERR(VALIDATION_PREFIX "A layout must be a JSON array");
        return false;
    }

    size_t index;
    json_t* entry;
    json_array_foreach(root, index, entry)
    {
        if (!json_is_object(entry)) {
            LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu was not a layout object", index);
            return false;
        }

        json_t* name = json_object_get(entry, "name");
        if (!json_is_string(name)) {
            LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu invalid name", index);
            return false;
        }

        const char* name_val = json_string_value(name);
        if (!ls_component_is_valid(name_val)) {
            LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu unknown component %s", index, name_val);
            return false;
        }

        json_t* enabled = json_object_get(entry, "enabled");
        if (!json_is_boolean(enabled)) {
            LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu invalid or missing enabled value", index);
            return false;
        }

        json_t* settings = json_object_get(entry, "settings");
        if (!settings) {
            // settings are optional
            continue;
        }

        if (!json_is_object(settings)) {
            LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu invalid settings entry", index);
            return false;
        }

        const char* key;
        json_t* val;
        json_object_foreach(settings, key, val)
        {
            if (!json_is_string(val) && !json_is_boolean(val) && !json_is_number(val)) {
                LOG_ERRF(VALIDATION_PREFIX, "Invalid entry at %zu invalid settings entry %s must be primitive and not null", index, key);
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Removes old component widgets from the previous layout and destroys them.
 *
 * @param win The current main app window.
 */
static void components_cleanup(LSAppWindow* win)
{
    GList* widgets = NULL;
    for (GList* elem = win->components; elem; elem = elem->next) {
        LSComponent* component = elem->data;
        if (!component) {
            continue;
        }

        GtkWidget* widget = component->ops->widget(component);
        if (widget) {
            widgets = g_list_append(widgets, g_object_ref(widget));
            gtk_box_remove(GTK_BOX(win->box), widget);
        }
    }

    g_list_free_full(win->components, ls_component_destroy);
    g_list_free_full(widgets, g_object_unref);
    win->components = NULL;
}

static void init_layout(LSAppWindow* win, json_t* layout)
{
    // Create all available components (TODO: change this in the future)
    LOG_DEBUG("Creating components...");
    if (win->components) {
        components_cleanup(win);
    }

    size_t index;
    json_t* entry;
    json_array_foreach(layout, index, entry)
    {
        if (json_is_false(json_object_get(entry, "enabled"))) {
            continue;
        }

        const char* name = json_string_value(json_object_get(entry, "name"));
        LSComponentAvailable* component_entry = ls_component_get(name);
        if (!component_entry) {
            // This should never happen since we already validated the layout.
            LOG_WARNF("Invalid component: %s", name);
            continue;
        }

        LSComponent* component = component_entry->new();
        if (component) {
            GtkWidget* widget = component->ops->widget(component);
            if (widget) {
                gtk_widget_set_margin_start(widget, WINDOW_PAD);
                gtk_widget_set_margin_end(widget, WINDOW_PAD);
                gtk_box_append(GTK_BOX(win->box), widget);
            }

            win->components = g_list_append(win->components, component);
        }
    }
}

bool ls_load_default_layout(LSAppWindow* win)
{
    GError* error = NULL;
    GBytes* data = g_resources_lookup_data(LIBRESPLIT_RESOURCES_PREFIX "layouts/default.json", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
    if (!data) {
        LOG_ERRF("Unable to load default layout: %s", error ? error->message : "unknown error");
        g_error_free(error);
        return false;
    }

    gsize length;
    const char* contents = g_bytes_get_data(data, &length);
    json_error_t json_error;
    json_t* layout = json_loadb(contents, length, 0, &json_error);
    if (!layout) {
        LOG_ERRF("Unable to load default layout: Invalid JSON at line %d: %s", json_error.line, json_error.text);
        return false;
    }

    if (!validate_layout(layout)) {
        json_decref(layout);
        return false;
    }

    init_layout(win, layout);
    json_decref(layout);
    return true;
}

bool ls_load_layout(LSAppWindow* win, const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return ls_load_default_layout(win);
    }

    struct stat st = { 0 };
    if (stat(path, &st) == -1) {
        LOG_INFOF("Layout at: '%s' does not exist", path);
        return ls_load_default_layout(win);
    }

    json_error_t json_error;
    json_t* layout = json_load_file(path, 0, &json_error);
    if (!layout) {
        LOG_ERRF("Unable to load default layout: Invalid JSON at line %d: %s", json_error.line, json_error.text);
        return false;
    }

    if (!validate_layout(layout)) {
        json_decref(layout);
        return false;
    }

    init_layout(win, layout);
    json_decref(layout);
    return true;
}
