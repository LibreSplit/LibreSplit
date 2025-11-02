# Themes

LibreSplit can be customized using themes, which are made of CSS stylesheets.

## Changing the theme

You can set the global theme by changing the `theme` value using `gsettings`.

```sh
gsettings set com.github.wins1ey.libresplit theme <theme-name>
```

Also each split JSON file can apply their own themes by specifying a `theme` key in the main object, see [the split files documentation](split-files.md) for more information.

## Creating your own theme

1. Create a CSS stylesheet with your desired styles.
2. Place the stylesheet under the `~/.config/libresplit/themes/<name>/<name>.css`directory where `name` is the name of your theme. If you have your `XDG_CONFIG_HOME` env var pointing somewhere else, you may need to change the directory accordingly.
3. Theme variants should follow the pattern `<name>-<variant>.css`.

See the [GtkCssProvider documentation](https://docs.gtk.org/gtk3/css-properties.html) for a list of supported CSS properties. Note that you can also modify the default font-family.

| LibreSplit CSS classes        | Explanation Where needed                        |
| ----------------------------- | ----------------------------------------------- |
| `.window`                     |                                                 |
| `.header`                     |                                                 |
| `.title`                      |                                                 |
| `.attempt-count`              |                                                 |
| `.time`                       |                                                 |
| `.delta`                      |                                                 |
| `.time`                       |                                                 |
| `.timer`                      |                                                 |
| `.timer-seconds`              |                                                 |
| `.timer-millis`               |                                                 |
| `.delay`                      | Timer not running/in negative time              |
| `.splits`                     | Container of the splits                         |
| `.split`                      | The splits themselves                           |
| `.current-split`              |                                                 |
| `.split-title`                |                                                 |
| `.split-icon`                 |                                                 |
| `.split-time`                 |                                                 |
| `.split-delta`                | Comparison time in the split                    |
| `.split-last`                 | The last split, if its not yet scrolled down to |
| `.done`                       |                                                 |
| `.behind`                     | Behind the PB but gaining time                  |
| `.losing`                     | Ahead of PB but losing time                     |
| `.behind.losing`              | (class combination) Behind PB and losing time   |
| `.best-segment`               |                                                 |
| `.best-split`                 |                                                 |
| `.footer`                     |                                                 |
| `.prev-segment-label`         |                                                 |
| `.prev-segment`               |                                                 |
| `.segment`                    |                                                 |
| `.segment-best`               |                                                 |
| `.segment-pb`                 |                                                 |
| `.segment-seconds`            |                                                 |
| `.segment-millis`             |                                                 |
| `.sum-of-bests-label`         |                                                 |
| `.sum-of-bests`               |                                                 |
| `.split-icon`                 |                                                 |
| `.personal-best-label`        |                                                 |
| `.personal-best`              |                                                 |
| `.world-record-label`         |                                                 |
| `.world-record`               |                                                 |

If a split has a `title` key, its UI element receives a class name derived from its title.

Specifically, the title is lowercase and all non-alphanumeric characters are replaced with hyphens, and the result is concatenated with `split-title-`.

For instance, if your split is titled "First split", it can be styled by targeting the CSS class `.split-title-first-split`.
