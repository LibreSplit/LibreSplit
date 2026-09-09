#include "plugins/plugin_loading.h"
#include "gtk/gtk.h"
#include "logging.h"
#include "plugins/plugin_utils.h"
#include "settings/utils.h"
#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PlugAPI api = {
    .abi_version = 1, // v0.1
    .register_lua_function = register_lua_function,
    .register_event_hook = register_event_hook,
    .register_component = register_plugin_component,
};

static PluginRegistry plugin_registry = {
    .count = 0,
    .size = 2,
    .plugins = NULL,
    .enabled = false,
};

const static size_t PLUGIN_VERSION_LENGTH = 21; /*!< 10 char for major, 10 for minor + 1 for dot */

/**
 * Does a check for ABI compatibility against the host ABI.
 *
 * @param[in] plugin_version The plugin ABI version.
 *
 * @returns -1 if there is a major incompatibility, 1 if there is a minor incompatibility, 0 otherwise.
 */
static int check_abi_version(abi_version_t plugin_version)
{
    uint16_t plug_major = plugin_version >> 16;
    uint16_t plug_minor = plugin_version & 0xFFFF;
    uint16_t host_major = api.abi_version >> 16;
    uint16_t host_minor = api.abi_version & 0xFFFF;
    if (plug_major != host_major) {
        LOG_WARN("Found Incompatible major ABI versions");
        return -1;
    }
    if (plug_minor > host_minor) {
        LOG_WARN("Plugin has a higher minor ABI version, some required functionality may be missing");
        return 1;
    }
    return 0;
}

/**
 * Utility function to convert a uint32 version into a readable one
 *
 * @param[in] ver The version to convert to string
 * @param[out] buffer The buffer to write the version onto
 * @param[in] buffer_size The size of the output buffer.
 */
static int print_version(const abi_version_t ver, char* buffer, size_t buffer_size)
{
    uint16_t major = ver >> 16;
    uint16_t minor = ver & 0xFFFF;
    if (buffer_size < PLUGIN_VERSION_LENGTH) {
        LOG_WARN("Buffer for writing version is too small");
        return -1;
    }
    int n = snprintf(buffer, buffer_size, "%u.%u", major, minor);
    if (n < 0 || (size_t)n >= buffer_size) {
        // Failed allocation or truncation
        return -1;
    }
    return 0;
}

/**
 * Does a check on the metadata passed, and eventually prints an error.
 *
 * @param[in] path The path of the plugin
 * @param[in] property The property to be checked
 * @param[in] property_name The name of the the checked property, used to print messages in logs.
 *
 * @returns 0 if everything is okay. -1 otherwise.
 */
static int check_metadata(const char* path, const char* property, const char* property_name)
{
    if (!property) {
        const char* err = dlerror();
        if (err) {
            LOG_WARNF("Cannot load plugin %s from metadata for plugin: %s - %s", property_name, path, err);
        } else {
            LOG_WARNF("Cannot load plugin %s from metadata for plugin: %s", property_name, path);
        }
        return -1;
    }
    return 0;
}

/**
 * Extracts the plugin metadata from the shared object without executing its code.
 *
 * @param[in] path The path to the plugin
 * @param[out] name The name of the plugin
 * @param[out] description The description of the plugin
 * @param[out] version The version of the plugin
 * @param[out] author The author of the plugin
 *
 * @returns 0 if everything went well. An error code otherwise.
 */
static int get_plugin_metadata(const char* path, char** name, char** description, char** version, char** author)
{
    // Loads the plugin lazily, not executing any code
    void* plugin_handle = dlopen(path, RTLD_LAZY);
    if (!plugin_handle) {
        LOG_WARNF("Unable to open plugin for inspection: %s - ", path, dlerror());
        return -1;
    }

    const abi_version_t* plugin_abi_version = (const abi_version_t*)dlsym(plugin_handle, "abi_version");

    if (!plugin_abi_version) {
        LOG_WARNF("Plugin missing required ABI version metadata: %s", path);
        dlclose(plugin_handle);
        return -1;
    }

    if (check_abi_version(*plugin_abi_version) < 0) {
        char abi_version[PLUGIN_VERSION_LENGTH];
        char host_version[PLUGIN_VERSION_LENGTH];
        print_version(*plugin_abi_version, abi_version, sizeof(abi_version));
        print_version(api.abi_version, host_version, sizeof(host_version));
        LOG_WARNF(
            "Plugin %s has incompatible ABI requirements: Host has %s, plugin supports %s",
            path,
            host_version,
            abi_version)
        dlclose(plugin_handle);
        return -1;
    }

    const char* plugin_name = (const char*)dlsym(plugin_handle, "plugin_name");
    if (check_metadata(path, plugin_name, "name") != 0) {
        dlclose(plugin_handle);
        return -1;
    }
    const char* plugin_desc = (const char*)dlsym(plugin_handle, "plugin_description");
    if (check_metadata(path, plugin_desc, "description") != 0) {
        dlclose(plugin_handle);
        return -1;
    }
    const char* plugin_version = (const char*)dlsym(plugin_handle, "plugin_version");
    if (check_metadata(path, plugin_version, "version") != 0) {
        dlclose(plugin_handle);
        return -1;
    }
    const char* plugin_author = (const char*)dlsym(plugin_handle, "plugin_author");
    if (check_metadata(path, plugin_author, "author") != 0) {
        dlclose(plugin_handle);
        return -1;
    }

    *name = strdup(plugin_name);
    if (!*name) {
        goto fail_metadata;
    }
    *description = strdup(plugin_desc);
    if (!*description) {
        goto fail_metadata;
    }
    *version = strdup(plugin_version);
    if (!*version) {
        goto fail_metadata;
    }
    *author = strdup(plugin_author);
    if (!*author) {
        goto fail_metadata;
    }

    dlclose(plugin_handle);
    return 0;

fail_metadata:
    if (*name) {
        free(*name);
        *name = NULL;
    }
    if (*description) {
        free(*description);
        *description = NULL;
    }
    if (*version) {
        free(*version);
        *version = NULL;
    }
    if (*author) {
        free(*author);
        *author = NULL;
    }
    dlclose(plugin_handle);
    return -1;
}

/**
 * Takes a path to a .so file and calls its "plug_init" function.
 *
 * @param[in] path The path to the plugin.
 *
 * @returns 0 if everything goes well or an error code otherwise.
 */
static int initialize_plugin(const char* path)
{
    if (!plugin_registry.plugins) {
        LOG_ERR("Plugin registry has not been initialized");
        return -1;
    }
    void* handle = NULL;
    Plugin tmp = {
        .handle = NULL,
        .path = NULL,
    };
    assert(plugin_registry.count <= plugin_registry.size);
    if (plugin_registry.count >= plugin_registry.size) {
        size_t new_size = plugin_registry.size * 2;
        Plugin* tmp_registry = realloc(plugin_registry.plugins, new_size * sizeof(Plugin));
        if (!tmp_registry) {
            LOG_ERR("Cannot reallocate memory for plugin registry");
            goto fail;
        }
        plugin_registry.plugins = tmp_registry;
        plugin_registry.size = new_size;
    }
    handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        LOG_WARNF("Unable to open plugin %s: %s", path, dlerror());
        goto fail;
    }

    // Purge stale errors
    dlerror();
    void* sym = dlsym(handle, "plug_init");
    const char* err = dlerror();
    if (err) {
        LOG_WARNF("Error while loading plugin initialization function symbol: %s", err);
    }

    union init_fn_ptr u;
    u.obj = sym;
    plugin_init_fn init_function = u.fn;

    if (!init_function) {
        LOG_WARNF("Cannot load plugin init function: %s", path);
        goto fail;
    }

    if (init_function(&api) != 0) {
        LOG_WARNF("Plugin init function failed: %s", path);
        goto fail;
    }

    tmp.handle = handle;
    tmp.path = strdup(path);

    if (!tmp.path) {
        LOG_WARNF("Cannot allocate memory for plugin path: %s", path);
        goto fail;
    }

    // NOTE: [Penaz] [2026-04-14] Inverting the following 2 lines will trigger
    // ^ the GCC analyzer, cause it will see the handle slot being assigned
    //  while the path may not transfer over, "leaking" the path strdup.
    plugin_registry.plugins[plugin_registry.count].path = tmp.path;
    plugin_registry.plugins[plugin_registry.count].handle = tmp.handle;
    // Make sure GCC's analyzer can see the ownership transfer
    tmp.handle = NULL;
    tmp.path = NULL;

    plugin_registry.count++;

    return 0;

fail:
    if (tmp.path) {
        free(tmp.path);
    }
    if (handle) {
        dlclose(handle);
        tmp.handle = NULL;
    }
    return -1;
}

/**
 * General function that loads plugins
 */
void load_plugins(void)
{
    int initialized = initialize_plugin_registry();
    if (initialized != 0) {
        LOG_ERR("There has been an error initializing the plugin system. The plugin system will be disabled.")
        return;
    }
    char plugdir[PATH_MAX];
    get_libresplit_data_folder_path(plugdir);
    strlcat(plugdir, "/plugins/", sizeof(plugdir));
    LOG_DEBUGF("Plugin Directory is: %s", plugdir);
    DIR* dir = opendir(plugdir);
    if (!dir) {
        LOG_WARNF("Unable to open plugins directory: %s", plugdir);
        return;
    }

    struct dirent* ent;

    while ((ent = readdir(dir))) {
        const char* extension = strrchr(ent->d_name, '.');
        if (!extension || strcmp(extension, ".so") != 0) {
            LOG_DEBUGF("Ignoring non-plugin %s", ent->d_name);
            continue;
        }

        char path[PATH_MAX];
        // If we didn't reduce the plugdir buffer size, GCC would complain about
        // possible buffer overflows here.
        if (strlen(plugdir) + sizeof(ent->d_name) + 1 > PATH_MAX) {
            LOG_WARN("Could not copy plugin path. Path may be too long.");
            return;
        }
        ssize_t n = snprintf(path, sizeof(path), "%s/%s", plugdir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(plugdir)) {
            LOG_WARN("Could not copy plugin path or buffer overflow detected.");
            return;
        }

        // Initialize the metadata
        char *name = NULL, *desc = NULL, *version = NULL, *author = NULL;
        if (get_plugin_metadata(path, &name, &desc, &version, &author) == 0) {
            LOG_INFOF("Found plugin: %s (v%s) by %s - %s", name, version, author, desc);
            free(name);
            free(desc);
            free(version);
            free(author);
            // TODO: [Penaz] [2026-03-14] Remember to dlclose at the end
            initialize_plugin(path);
        }
    }
    closedir(dir);
}

/**
 * Prepares the plugin registry to be used.
 *
 * @returns Zero if everything went well. An error code otherwise.
 */
int initialize_plugin_registry(void)
{
    LOG_INFO("Initializing plugin registry");
    if (plugin_registry.enabled) {
        LOG_INFO("Plugin registry already initialized");
        return 0;
    }
    plugin_registry.plugins = malloc(plugin_registry.size * sizeof(Plugin));
    if (!plugin_registry.plugins) {
        LOG_WARN("Plugin registry initialization failed (malloc failed).");
        return -1;
    }
    plugin_registry.enabled = true;
    return 0;
}

/**
 * Closes the handlers for all plugins and frees memory-
 *
 * @returns Zero if everything went well, an error code otherwise
 */
int unload_plugins(void)
{
    LOG_INFO("Closing plugin handlers");
    for (int i = 0; i < plugin_registry.count; i++) {
        if (plugin_registry.plugins[i].path) {
            LOG_DEBUGF("Calling shutdown function for plugin %s", plugin_registry.plugins[i].path);
        } else {
            LOG_DEBUG("Calling shutdown function for unknown plugin");
        }
        // Purge stale errors
        dlerror();
        void* sym = dlsym(plugin_registry.plugins[i].handle, "plug_shutdown");
        const char* err = dlerror();
        if (err) {
            LOG_WARNF("Loading plugin shutdown symbol failed: %s", err);
        }
        union shutdown_fn_ptr u;
        u.obj = sym;
        plugin_shutdown_fn shutdown_function = u.fn;
        if (!shutdown_function) {
            if (plugin_registry.plugins[i].path) {
                LOG_WARNF("No shutdown function defined for plugin %s", plugin_registry.plugins[i].path);
            } else {
                LOG_WARN("No shutdown function defined for unknown plugin.")
            }
        } else {
            if (shutdown_function() != 0) {
                if (plugin_registry.plugins[i].path) {
                    LOG_WARNF("Shutdown function for plugin %s failed", plugin_registry.plugins[i].path);
                } else {
                    LOG_WARN("Shutdown function failed for unknown plugin.")
                }
            }
        }
        if (plugin_registry.plugins[i].path) {
            LOG_DEBUGF("Closing handlers for plugin %s", plugin_registry.plugins[i].path);
        } else {
            LOG_DEBUG("Closing handlers for unknown plugin");
        }
        // Close the dynamic linking handler
        if (plugin_registry.plugins[i].handle) {
            dlclose(plugin_registry.plugins[i].handle);
        }
        plugin_registry.plugins[i].handle = NULL;
        free(plugin_registry.plugins[i].path);
    }
    free(plugin_registry.plugins);
    plugin_registry.size = 2;
    plugin_registry.count = 0;
    plugin_registry.plugins = NULL;
    plugin_registry.enabled = false;
    return 0;
}

/**
 * Calls, in turn, all the register_context_menu functions
 * of the plugins, to fill in the Plugins submenu.
 *
 * @param parent The Plugins submenu.
 * @returns 0 if everything went well. An error code otherwise.
 */
int create_plugin_context_menus(GtkWidget* parent)
{
    // XXX: [Penaz] [2026-08-23] As things are now, each plugin has full control
    // ^ over the "Plugins" submenu. Ideally we would want to isolate each plugin
    // ^ into its own submenu to reduce interactions.
    GtkWidget* submenu = gtk_menu_new();
    if (plugin_registry.enabled) {
        for (int i = 0; i < plugin_registry.count; i++) {
            // Purge stale errors
            dlerror();
            void* sym = dlsym(plugin_registry.plugins[i].handle, "register_context_menu");
            const char* err = dlerror();
            if (err) {
                LOG_WARNF("Error while loading plugin context menu registration function symbol: %s", err);
                continue;
            }

            union context_menu_fn_ptr u;
            u.obj = sym;
            context_menu_reg_fn registration_function = u.fn;

            if (!registration_function) {
                LOG_WARN("Cannot load plugin context menu registration function");
                continue;
            }

            // Register the submenu item
            int result = registration_function(submenu);

            if (result != 0) {
                LOG_WARNF("Plugin context menu registration function returned exit code %s", result);
            }
        }
    }
    GList* submenu_children = gtk_container_get_children(GTK_CONTAINER(submenu));
    if (submenu_children == NULL) {
        // If, by the end of the registration, there are no items in the "Plugins"
        // submenu, just fill it with a placeholder entry.
        GtkWidget* placeholder = gtk_menu_item_new_with_label("No plugin entries.");
        gtk_widget_set_sensitive(placeholder, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), placeholder);
    }
    // Set the newly created submenu to the "Plugins" entry in the context menu
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), submenu);
    return 0;
}
