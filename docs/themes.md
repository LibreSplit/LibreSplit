# Themes

LibreSplit can be customized using themes, which are made of CSS stylesheets.

## Changing the theme

You can set the global theme by changing the `theme` value in your `settings.json` configuration file, usually you'll find it in `~/.config/libresplit/`.

Also each split JSON file can apply their own themes by specifying a `theme` key in the main object, see [the split files documentation](split-files.md) for more information.

## Creating your own theme

1. Create a CSS stylesheet with your desired styles.
2. Place the stylesheet under the `~/.config/libresplit/themes/<name>/<name>.css`directory where `name` is the name of your theme. If you have your `XDG_CONFIG_HOME` env var pointing somewhere else, you may need to change the directory accordingly.
3. Theme variants should follow the pattern `<name>-<variant>.css`.

See the [GtkCssProvider documentation](https://docs.gtk.org/gtk3/css-properties.html) for a list of supported CSS properties. Note that you can also modify the default font-family.

| LibreSplit CSS classes        | Explanation Where needed                                                                                                                                 |
| ----------------------------- | -----------------------------------------------                                                                                                          |
| `.window`                     | Any window of LibreSplit (editing this class might affect other LibreSplit windows)                                                                      |
| `.window.main-window`         | LibreSplit's main timer window (this won't influence other LibreSplit windows)                                                                           |
| `.header`                     | The header, containing title and attempt counters                                                                                                        |
| `.title`                      | The title of the category (set in the splits json file)                                                                                                  |
| `.attempt-count`              | Counter for attempts in top right corner (#attempts_finished / attempts_total)                                                                           |
| `.delta`                      | Difference between current time and PB, appears at -30s to PB. Should be overwritten by .behind, .losing, .behind.losing, .best-split, and .best-segment |
| `.time`                       | All time fields from the main big timer, the splits times, and the times of personal best and sum of bests and other things in the footer                |
| `.timer`                      | The large main timer                                                                                                                                     |
| `.timer-container`            | Container for both the detailed and normal timer                                                                                                         |
| `.detailed-timer`             | Container for the `segment-pb` and `segment-best` classes                                                                                                |
| `.timer-seconds`              | Large main timer seconds                                                                                                                                 |
| `.timer-millis`               | Large main timer milli(seconds)                                                                                                                          |
| `.delay`                      | Timer not running/in negative time                                                                                                                       |
| `.splits`                     | Container of the splits                                                                                                                                  |
| `.split`                      | The splits themselves                                                                                                                                    |
| `.current-split`              | The split you're currently on                                                                                                                            |
| `.split-title`                | Name of the splits                                                                                                                                       |
| `.split-icon`                 | Icon for splits                                                                                                                                          |
| `.split-time`                 | Time for the splits                                                                                                                                      |
| `.split-delta`                | Comparison time in the split                                                                                                                             |
| `.split-last`                 | The last split, if its not yet scrolled down to                                                                                                          |
| `.done`                       | Colour of split timer after that split has been completed                                                                                                |
| `.behind`                     | Behind the PB but gaining time                                                                                                                           |
| `.losing`                     | Ahead of PB but losing time                                                                                                                              |
| `.behind.losing`              | (class combination) Behind PB and losing time                                                                                                            |
| `.best-segment`               | Fastest time in that segment                                                                                                                             |
| `.best-split`                 | Fastest pace to that point                                                                                                                               |
| `.footer`                     | A generic footer that contains sum of bests, prev-segment comparison, world record, and others                                                           |
| `.sum-of-bests-container`     | The container for the "sum of bests" label and timer                                                                                                     |
| `.personal-best-container`    | The container for the "personal best" label and timer                                                                                                    |
| `.prev-segment-container`     | The container for the "previous segment" label and timer                                                                                                 |
| `.world-record-container`     | The container for the "world record" label and timer                                                                                                     |
| `.prev-segment-label`         | Text that says "Previous Segment" and changes to "Live Segment"                                                                                          |
| `.prev-segment`               | Time for Previous Segment (and Live Segment)                                                                                                             |
| `.segment`                    | Smaller detailed timer for current segment                                                                                                               |
| `.segment-best`               | Best text within .detailed-timer                                                                                                                         |
| `.segment-pb`                 | PB text within .detailed-timer                                                                                                                           |
| `.segment-seconds`            | Smaller detailed timer seconds                                                                                                                           |
| `.segment-millis`             | Smaller detailed timer milli(seconds)                                                                                                                    |
| `.sum-of-bests-label`         | Text that says "Sum of Bests"                                                                                                                            |
| `.sum-of-bests`               | Time for Sum of Bests                                                                                                                                    |
| `.personal-best-label`        | Text that says "Personal Best"                                                                                                                           |
| `.personal-best`              | Time for Personal Best                                                                                                                                   |
| `.world-record-label`         | Text that says "World Record"                                                                                                                            |
| `.world-record`               | Time for World Record                                                                                                                                    |

If a split has a `title` key, its UI element receives a class name derived from its title.

Specifically, the title is lowercase and all non-alphanumeric characters are replaced with hyphens, and the result is concatenated with `split-title-`.

For instance, if your split is titled "First split", it can be styled by targeting the CSS class `.split-title-first-split`.

A more complex example: if your split is named "Space Station (Part 1)", the CSS class will be `.split-title-space-station--part-1-` (because the parentheses will become hyphens).

## FAQ

### How do I hide a section of LibreSplit?

GTK does not have a built-in way of hiding pieces of the interface, but you can hide most items by setting the font-size to zero. For instance:

```css
.segment-pb, .segment-best{
    font-size: 0;
}
```

Will hide the PB and Best sections.

### Is there anything to help me develop themes out there?

In fact, there is!

If you run LibreSplit from a terminal like this:

```sh
GTK_DEBUG=interactive libresplit
```

LiveSplit will be started together with another window: the interactive GTK debugger. Like the one you see below:

![The GTK Debug window](./images/gtk_debugger.png)

Make sure that both LibreSplit and this window are visible, because when you click on one row in the GTK debugger (`Objects` tab), the corresponding section in LiveSplit will flash 3 times, letting you know what you selected.

Once you found what you want to edit, take a note of its `style class` (See [Creating your own theme](#creating-your-own-theme) for a list) and head to the `CSS` tab: there you can edit in real time LiveSplit's aspect. These edits are temporary, but they can help you developing your own CSS theme.

Once you're done developing your theme, feel free to share it with the community!


### Can I customize LibreSplit's look even further?

Yes! But you'll need to first erase all your desktop theme's (and LibreSplit's) rules, so your theme will have to start with the following rule:

```
* {
    all: unset;
}
```

**WARNING:** If you use this rule without setting any others, LibreSplit won't look like a GTK (or an application) at all, and will be next to unusable.

With the help of GTK's [CSS Overview](https://docs.gtk.org/gtk3/css-overview.html) you'll be able to customize everything, including Context Menus, The Help and Settings screens (to an extent).

This is very advanced wizardry, so be careful!
