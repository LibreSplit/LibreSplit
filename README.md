<p align="center">
<img src="assets/libresplit.svg" width="100" height="100" align="top"/>
</p>
<h1 align="center">LibreSplit</h1>

<p align="center">
<img src="https://img.shields.io/badge/Code-00599C?style=for-the-badge&logo=c&logoColor=white"</img>
<a href="https://github.com/LibreSplit/LibreSplit-resources/tree/main/auto-splitters">
<img src="https://img.shields.io/badge/Auto%20Splitters-000081?style=for-the-badge&logo=lua&logoColor=white"</img>
</a>
<a href="https://github.com/LibreSplit/LibreSplit/stargazers">
<img src="https://img.shields.io/github/stars/LibreSplit/LibreSplit?style=for-the-badge&logo=GitHub"</img>
</a>
<a href="https://docs.gtk.org/gtk3/getting_started.html">
<img src="https://img.shields.io/static/v1?label=Made%20with&message=GTK%203.0&color=725d9c&style=for-the-badge&logo=GTK&logoColor=white"/>
</a>
<a href="https://github.com/LibreSplit/LibreSplit/blob/main/LICENSE">
<img src="https://img.shields.io/github/license/LibreSplit/LibreSplit?label=license&style=for-the-badge&logo=GNU&logoColor=white&color=b85353"/>
</a>
<p>

<p align="center">
<a href="https://discord.gg/qbzD7MBjyw"><img src="https://img.shields.io/discord/1381914148585078804?style=for-the-badge&logo=Discord&label=LibreSplit&color=7289da"</img></a>
<a href="https://github.com/LibreSplit/LibreSplit/actions"><img src="https://img.shields.io/github/actions/workflow/status/LibreSplit/LibreSplit/build.yml?style=for-the-badge&logo=GitHub"</img></a>
</p>

## About

LibreSplit is a speedrun timer based on [urn](https://github.com/3snowp7im/urn) that adds support for Lua-based auto splitters that are easy to port from ASL.

<p align="center">
<img src="https://github.com/LibreSplit/LibreSplit-resources/blob/main/themes/standard/standard.png" width="45%" alt="Standard Theme: A grey theme with bright coloured timer background. Uses a sans serif font, has a bright pink border around the whole window">
<img src="https://github.com/LibreSplit/LibreSplit-resources/blob/main/themes/minimalistic/minimalistic.png" width="45%" alt="Minimalistic Theme: A dark theme with bright curved edge outlines. Uses a thick sans serif font.">
</p>

### If you are looking for the public repository of splits, auto splitters and themes. They are located [here](https://github.com/LibreSplit/LibreSplit-resources)

## Features

- **Split Tracking and Timing:** Accurately track and time your speedruns with ease.
- **Auto Splitter Support:** Utilize Lua-based auto splitters to automate split timing based on in-game events.
- **Customizable Themes:** Customize your timer's appearance by creating and applying custom themes.
- **Flexible Configuration:** Configure keybindings and various settings to suit your preferences.
- **Icon support for splits.**
- **Always on Top support.**
- **Support for in-game time.**

----

## Installation

- Arch-based Distros
    - `yay libresplit-git`
    - `paru libresplit-git`

    See the [libresplit-git](https://aur.archlinux.org/packages/libresplit-git) package on the Arch User Repository (AUR).

- NixOS

    See the [libresplit](https://search.nixos.org/packages?channel=25.05&show=libresplit&query=libresplit) package, courtesy of [@fgaz](https://github.com/fgaz).

## Building

LibreSplit requires the following dependencies on your system to compile:

- `git`
- `meson`
- `libgtk+-3.0`
- `x11`
- `libjansson`
- `luajit`

Install the required dependencies:

- Debian-based systems

    ```sh
    sudo apt update
    sudo apt install build-essential libgtk-3-dev libjansson-dev meson libluajit-5.1-dev cmake
    ```

- Arch-based systems

    ```sh
    sudo pacman -Sy
    sudo pacman -S gtk3 jansson luajit git meson
    ```

Clone the project:

```sh
git clone https://github.com/LibreSplit/LibreSplit
cd LibreSplit
```

Now compile and install:

```sh
meson setup build -Dbuildtype=release
meson compile -C build
meson install -C build
```

All done! Now you can start the desktop **LibreSplit** or run `/usr/local/bin/libresplit`.

---

## Usage

When you start **LibreSplit** for the first time, it will create the `libresplit` directory in your config directory (it will usually be `~/.config/libresplit`). Such directory will contain:

- Splits.
- Auto Splitters.
- Themes.

All 3 directories will start empty, so you may want to download the [resource repository](https://github.com/LibreSplit/LibreSplit-resources/) first and clone it in `~/.config/libresplit/` before starting LibreSplit for the first time.

A file dialog will then appear, asking you to select a Split JSON file (see [Split files](#split-files)).

Initially the window is undecorated. You can toggle window decorations by pressing the `Right Control` key.

### Default Keybinds

The timer is controlled with the following keys
(note that their action **depends on the state of the timer**):

| Key                   | Timer is Stopped   | Timer is running   |
| --------------------- | ------------------ | ------------------ |
| <kbd>Spacebar</kbd>   | Start timer        | Split              |
| <kbd>Backspace</kbd>  | Reset timer        | Stop timer         |
| <kbd>Delete</kbd>     | Cancel             | -                  |

Cancel will **reset the timer** and **decrement the attempt counter**. A run that is reset before the start delay is automatically cancelled.

If you forget to split, or accidentally split twice, you can manually change the current split:

| Key                  | Action     |
| -------------------- | ---------- |
| <kbd>Page Up</kbd>   | Unsplit    |
| <kbd>Page Down</kbd> | Skip Split |

### Colors

The color of a time or delta has a special meaning.

| Color         | Meaning                                  |
| ------------- | ---------------------------------------- |
| Dark red      | Behind splits in PB and losing time      |
| Light red     | Behind splits in PB and gaining time     |
| Dark green    | Ahead of splits in PB and gaining time   |
| Light green   | Ahead of splits in PB and losing time    |
| Blue          | Best split time in any run               |
| Gold          | Best segment time in any run             |

---

## Settings and Keybinds

If you want to change the default settings or keybinds, you can check the [Settings and Keybinds documentation](docs/settings-keybinds.md)

---

## Auto Splitters

LibreSplit supports auto splitters written in Lua to automate split timing based on in-game events.

Feel free to make your own, Documentation can be found [here](docs/auto-splitters.md)

---

## Split Files

Split files in LibreSplit are stored as well-formed JSON.

Check the [Split Files documentation](docs/split-files.md) for more information.

---

## Themes

You can customize LibreSplit to your liking using themes.

For more information, check the [Themes documentation](docs/themes.md).

---

## FAQ

- **How do I resize the application window?**

    You can edit the `width` and `height` properties in the [Split JSON File](docs/split-files.md)

- **How do I change the default keybinds?**

    You can change the keybinds by editing your `settings.json` file (usually inside the `~/.config/libresplit` folder).

    See [Settings and Keybinds](docs/settings-keybinds.md) for some examples and more information.

- **How do I make the keybinds global?**

    You can set the `global_hotkeys` property as `true` by editing your `settings.json` file.

    Wayland users experienced crashes when enabled `global_hotkeys`, so this settings is ignored for Wayland desktops.

    See [Settings and Keybinds](docs/settings-keybinds.md) for more information and workarounds for Wayland.

- **Can I modify LibreSplit's appearance?**

    Yes. You can [create your own theme](docs/themes.md) or [download themes online](https://github.com/LibreSplit/LibreSplit-resources/tree/main/themes).

- **How can I make my own split file?**

    You can use any existing JSON split file as an example from our [resource repository](https://github.com/LibreSplit/LibreSplit-resources/tree/main/splits) and refer to the [Split Files Documentation](docs/split-files.md) for more information.

    You can place the split files wherever you prefer, just open them when starting LibreSplit.

- **Can I define custom icons for my splits?**

    Yes! You can use local files or web urls. See the `icon` key in the [split object](docs/split-files.md#split-object).

    The default icon size is 20x20px, but you can change it like so:

    ```css
    .split-icon {
        min-width: 24px;
        min-height: 24px;
        background-size: 24px;
    }
    ```

- **Can I contribute?**

    Absolutely!

    You can contribute in many ways:

    - By making [pull requests](https://github.com/LibreSplit/LibreSplit/pulls).
    - By creating new themes, split files or auto splitters and add them to our [resource repository](https://github.com/LibreSplit/LibreSplit-resources/).
    - By [reporting issues](https://github.com/LibreSplit/LibreSplit/pulls).
    - By sending us suggestions, feature requests, improve the documentation and more. Feel free to join our [discord server](https://discord.gg/qbzD7MBjyw) to follow LibreSplit's development up close.

---

## Uninstall LibreSplit

You can uninstall LibreSplit using your package manager or, if you built it manually, by running

```sh
cd build
sudo ninja uninstall
```

## Star History

<a href="https://www.star-history.com/#LibreSplit/LibreSplit&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=LibreSplit/LibreSplit&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=LibreSplit/LibreSplit&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=LibreSplit/LibreSplit&type=date&legend=top-left" />
 </picture>
</a>
