# Split Files

Split files are stores as well-formed JSON and **must** contain one main object.

You can use splits located in [the resource repository](https://github.com/LibreSplit/LibreSplit-resources/tree/main/splits) to start creating your own split files and place them however you want.

## Main Object

| Key             | Type               | Value                                   |
| --------------- | ------------------ | --------------------------------------- |
| `title`         | string             | Title string at top of window           |
| `attempt_count` | int                | Number of attempts                      |
| `start_delay`   | string (timestamp) | Non-negative delay until timer starts   |
| `world_record`  | string             | Best known time                         |
| `splits`        | array              | Array of [split objects](#split-object) |
| `theme`         | string             | Window theme                            |
| `theme_variant` | string             | Window theme variant                    |
| `width`         | int                | Window width                            |
| `height`        | int                | Window height                           |

Most of the above keys are optional.

## Split Object

| Key            | Type   | Value                                                     |
| -------------- | ------ | --------------------------------------------------------- |
| `title`        | string | Split title                                               |
| `icon`         | string | Icon file path or url                                     |
| `time`         | string | Split time - the total time up to this segment in your PB |
| `best_time`    | string | Your best split time                                      |
| `best_segment` | string | Your best segment time (split gold)                       |

Times are strings in `HH:MM:SS.mmmmmm` format.

Icons can be either a local file path (preferably absolute) or a URL. Note that only GTK-supported image formats will work. For example, `.svg` and `.webp` won't.

## Subsplit Groups

Splits can be grouped into subsplit groups using special prefixes in the `title` field:

- **Normal split**: A title with no prefix is displayed as a regular split.
- **Subsplit item**: A title starting with `-` (e.g., `"- Map 1"`) marks that split as a subsplit item. It will be displayed indented under its group header. The `-` prefix is stripped from the display name.
- **Group ender**: A title starting with `{group_name}` (e.g., `"{Campaign} Finale"`) closes a subsplit group. The text inside the curly braces becomes the group header name, and the text after the braces becomes the split's display name. This split is also indented as part of the group.

A group consists of one or more consecutive `-` prefixed subsplit items followed by a `{group_name}` prefixed split. The group header displays the group name with the cumulative time for all splits in the group.

### Subsplit Example

```json
{
    "title": "Left 4 Dead 2",
    "splits": [
        { "title": "- The Port" },
        { "title": "- The Projects" },
        { "title": "- The Shithouse" },
        { "title": "- The Cemetery" },
        { "title": "{Dead Center} The Atrium" },
        { "title": "- The Highway" },
        { "title": "- The Fairgrounds" },
        { "title": "- The Town" },
        { "title": "{Dark Carnival} The Concert" }
    ]
}
```

This will be displayed as:

```
Dead Center             12:34  +0:05
  The Port              02:30
  The Projects          05:15
  The Shithouse         08:45
  The Cemetery          11:00
  The Atrium            12:34
Dark Carnival           20:15  -1:20
  The Highway           14:30
  The Fairgrounds       16:45
  The Town              18:30
  The Concert           20:15
```

Subsplit rows are hidden until their group becomes active (when the current split reaches the first subsplit in the group). Once the group is completed, all subsplits are hidden again and only the group header remains, showing the total time and time gain/loss for the chapter.

## Example

Here is a quick example of how a simple split file would look:

```json
{
    "title": "School - Homework%",
    "attempt_count": 55,
    "splits": [
        {
            "title": "Maths",
            "time": "05:12:55.000000",
            "best_time": "04:10:50.000000",
            "best_segment": "04:10:50.000000",
        },
        {
            "title": "Science",
            "time": "07:36:30.000000",
            "best_time": "05:26:25.000000",
            "best_segment": "01:15:35.000000",
        }
    ],
    "width": 250,
    "height": 500
}
```

In the example above, the individual segment time for the "Maths" split would be `05:12:55.000000` and the individual segment time for the "Science" split would be `02:23:35.000000`