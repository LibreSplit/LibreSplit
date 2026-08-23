#pragma once
#include "plugins/plugin_utils.h"
#include <gtk/gtk.h>

/**
 * A plugin structure, keeps tabs on the handler
 * given by dlopen() and the path
 */
typedef struct _Plugin {
    char* path; /*!< Path of the loaded plugin */
    void* handle; /*!< Handle of the .so file, used to dlclose */
} Plugin;

/**
 * A plugin registry, used to keep in memory
 * handles to all plugins loaded.
 */
typedef struct _PluginRegistry {
    int count; /*!< Number of loaded and active plugins */
    int size; /*!< Size of the plugin registry */
    Plugin* plugins; /*!< Array of Plugin structs with all their handles. */
    bool enabled; /*!< Defines if the plugin system is fully enabled. */
} PluginRegistry;

/**
 * A "plugin init function" type.
 */
typedef int (*plugin_init_fn)(PlugAPI*);
typedef int (*plugin_shutdown_fn)(void);
typedef int (*context_menu_reg_fn)(GtkWidget*);

/**
 * Union used to convert between void* and plugin_init_fn
 * without violating ISO C standards.
 */
union init_fn_ptr {
    void* obj; /*!< Object pointer */
    plugin_init_fn fn; /*!< Function pointer */
};

/**
 * Union used to convert between void* and plugin_shutdown_fn
 * without violating ISO C standards.
 */
union shutdown_fn_ptr {
    void* obj; /*!< Object pointer */
    plugin_shutdown_fn fn; /*!< Function pointer */
};

/**
 * Union used to convert between void* and context_menu_reg_fn
 * without violating ISO C standards.
 */
union context_menu_fn_ptr {
    void* obj; /*!< Object pointer */
    context_menu_reg_fn fn; /*!< Function Pointer */
};

void load_plugins(void);
int unload_plugins(void);
int create_plugin_context_menus(GtkWidget* parent);

int initialize_plugin_registry(void);
