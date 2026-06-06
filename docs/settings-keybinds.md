# Settings and keybinds

LibreSplit uses JSON as its way to keep track of your preferences.

## The settings file

Settings are saved in the `settings.json` file inside your `XDG_CONFIG_HOME` folder (it will usually be situated in `~/.config/libresplit/settings.json`).

The settings file is divided in 3 main parts:

- **libresplit:** Which contains the general settings;
- **keybinds:** Which contains your key bindings;
- **history:** Which tracks the last splits, auto splitters, split folder and auto splitters folder you opened.

### General settings

Under the `libresplit` section, you will find the following settings:

| Setting            | Type    | Description                                                  | Default        |
| -----------------  | ------- | -----------------------------------------------------        | -------------- |
| `start_decorated`  | Boolean | Start with window decorations                                | `false`        |
| `start_on_top`     | Boolean | Start with window as always on top                           | `true`         |
| `hide_cursor`      | Boolean | Hide cursor in window                                        | `false`        |
| `global_hotkeys`   | Boolean | Enables global hotkeys                                       | `false`        |
| `start_on_top`     | Boolean | Start with window as always on top                           | `false`        |
| `theme`            | String  | Default theme name                                           | `"standard"`   |
| `theme_variant`    | String  | Default theme variant                                        | `""`           |
| `decimals`         | Integer | Number of decimals to show on the timer (from 0 to 6)        | `2`            |
| `save_run_history` | Boolean | Save JSON files with old runs in the runs subdirectory       | `true`         |
| `ask_on_gold`      | Boolean | Ask for confirmation before resetting a run with gold splits | `true`         |
| `ask_on_worse`     | Boolean | Ask before saving a run that is worse than PB                | `true`         |

### Keybind settings

Under the `keybind` section, you will find the following key bindings:

| Keybind                      | Type   | Description                                 | Default               |
| ---------------------------- | ------ | ------------------------------------------- | --------------------- |
| `start_split`                | String | Start/split keybind                         | space                 |
| `stop_reset`                 | String | Stop/Reset keybind                          | Backspace             |
| `cancel`                     | String | Cancel keybind                              | Delete                |
| `unsplit`                    | String | Unsplit keybind                             | Page_Up               |
| `skip_split`                 | String | Skip split keybind                          | Page_Down             |
| `toggle_decorations`         | String | Toggle window decorations keybind           | Control_R             |
| `toggle_win_on_top`          | String | Toggle window "Always on top" state keybind | &lt;Control&gt;k      |

Keybind strings must be parsable by the [gtk_accelerator_parse](https://docs.gtk.org/gtk4/func.accelerator_parse.html).

See the [complete list of keynames](https://github.com/GNOME/gtk/blob/main/gdk/keynames.txt) for `gdk`. Modifiers are enclosed in angular brackets <>: `<Shift>`, `<Ctrl>`, `<Alt>`, `<Meta>`, `<Super>`, `<Hyper>`. Note that you should use `<Alt>a` instead of `<Alt>-a` or similar.

## Modifying the default values

You can edit the settings by directly changing the `settings.json` file inside of LibreSplit's configuration directory.

Alternatively, you can use the settings Dialog by right clicking the LibreSplit window, and clicking on "Settings".
