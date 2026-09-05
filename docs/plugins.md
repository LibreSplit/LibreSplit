# Plugins

* Plugin system is in place, allowing you to add features to LibreSplit.

> [!IMPORTANT]
> Always make sure that you fully trust the plugins you're installing.
> Unknown plugins might contain malicious code and give you lots of headaches.

## How do I install a plugin?

Just drop the plugin (which should have a `.so` extension) in your `$XDG_DATA_HOME/libresplit/plugins/` directory (usually will be `~/.local/share/libresplit/plugins/`) and the plugin will be loaded on LibreSplit's next start.

To remove a plugin, just move it out of the `plugins` directory or delete it.

## Plugin Development

Plugins (so far) are enabled to:

- Register new Lua functions;
- Register handlers to timer events.

### The plugin metadata

Each plugin must have the following metadata to identify itself:

* name
* description
* version
* author
* ABI version of reference

You can just stick them in your plugin code like this:

```c
const char plugin_name[] = "Test Plugin";
const char plugin_description[] = "Does nothing, it just exists";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";
const abi_version_t abi_version = 0 << 16 | 1; // v0.1
```

The ABI version is a packed 32-bit integer, using 16 bits for the "major version" and the other 16 for the "minor version".

Here are some examples on how you can encode the ABI version.

```c
const abi_version_t abi_version = 0 << 16 | 1; // v0.1
const abi_version_t abi_version = 1 << 16 | 0; // v1.0
const abi_version_t abi_version = 1 << 16 | 4; // v1.4
```

If the major version between the ABI differs, LibreSplit will refuse the plugin, if the minor version in the plugin is higher than LibreSplit's, there will be a warning (because the plugin might refer to a newer ABI's functions that are not present).

### Initializing the plugin

Each plugin must have an entry point with the following signature:

```c
int plug_init(PlugAPI* api)
```

You can find `PlugAPI` in the `plugin.h` header file, as the plugin system is now, you'll need it to be able to register hooks and Lua functions.

In the init function you will have to register your hooks and Lua functions.

### Registering a new Lua function

Plugins can register new Lua functions. Such functions need to have a signature similar to this:

```c
int my_awesome_lua_function(lua_State* L)
```

You will need to register the function in the `plug_init` entry point, like follows:

```c
int plug_init(PlugAPI* api){
    api->register_lua_function("lua_function_name", my_awesome_lua_function);
    return 0;
}
```

Where `lua_function_name` is the name that will be used to call the function from inside the Lua Auto Splitter Runtime.

### Registering a new event handler

Plugins can register handlers for timer events. Such functions need to have a signature similar to this:

```c
int my_awesome_handler(const ls_timer* timer)
```

You will need to register the function in the `plug_init` entry point, like follows:

```c
int plug_init(PlugAPI* api){
    api->register_event_hook(EVENT, my_awesome_handler);
    return 0;
}
```

Where `EVENT` can be one of the following:

* `START` - Run on the start of the timer.
* `SPLIT` - Run on each split.
* `STOP` - Runs when the timer is stopped.
* `RESET` - Runs on a timer reset.
* `CANCEL` - Runs when the run is cancelled.
* `SKIP` - Runs every time a split is skipped.
* `UNSPLIT` - Runs every time a split is undone.
* `PAUSE` - Runs when the timer is paused.
* `UNPAUSE` - Runs when the timer is unpaused

### Registering a Context Menu Entry/Submenu

Plugins can register their own context (right click) menu entries. To do so, you can fill in the `register_context_menu` entry point with the code that takes care of building your own menu items and submenus.

Here's a quick example:

```c
int register_context_menu(GtkWidget* parent)
{
    GtkWidget* entry = gtk_menu_item_new_with_label("My Awesome Feature");
    g_signal_connect(entry, "activate", G_CALLBACK(my_awesome_handler), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(parent), entry);
    return 0;
}
```

### Registering a GUI Component

Plugins can register their own GUI components. To do so, you can see how a component is created in the LibreSplit source code and then call the registration function.

Here's a quick example:

```c
int plug_init(PlugAPI* api)
{
    api->register_component("just_a_string", ls_component_string_new);
    return 0;
}
```

### Closing the plugin

Each plugin should have a shutdown function with the following signature:

```c
int plug_shutdown()
```

This will be executed when LibreSplit is exiting, so you should use it to clean up resources and closing everything properly.
