---
title: Overview
description: Configure the overview mode for window navigation.
---

## Overview Settings

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `hotarea_size` | integer | `10` | Hot area size in pixels. |
| `enable_hotarea` | integer | `0` | Enable hot areas (0: disable, 1: enable). |
| `hotarea_disable_on_fullscreen` | integer | `1` | Disable hot areas while a fullscreen window is focused (0: disable, 1: enable). |
| `hotarea_corner` | integer | `2` | Hot area corner (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right). |
| `overviewgappi` | integer | `5` | Inner gap in overview mode. |
| `overviewgappo` | integer | `30` | Outer gap in overview mode. |
| `overcircle_center_ratio` | float | `0.5` | Width ratio of the centered window in the `overcircle` layout (0.1–0.9). |
| `jump_labels` | string | `HJKLASDFGQWERTYUIOPZXCVBNM` | Character sequence used for jump hints in overview mode. |
| `overview_group_by_tag` | integer | `0` | Group the overview by tag instead of mixing every tag's windows into one grid (0: disable, 1: enable). |
| `tagcolors` | string | 8-color built-in palette | Comma-separated list of `0xRRGGBBAA` colors, one per tag, cycled if there are more tags than colors. Used for the per-card tag badge/border when `overview_group_by_tag` is enabled. |

### Setting Descriptions

- `enable_hotarea` — Toggles overview when the cursor enters the configured corner.
- `hotarea_disable_on_fullscreen` — When enabled, the hot area does not trigger overview while a fullscreen window is focused.
- `hotarea_size` — Size of the hot area trigger zone in pixels.
- `hotarea_corner` — Corner that triggers the hot area (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right).
- `jump_labels` — Defines the ordered characters used for jump hints when in overview jump mode. Each visible window is assigned a label in this order, and pressing the corresponding key jumps to that window. The number of labels limits how many windows can be assigned hints at once.
- `overview_group_by_tag` — When enabled, the overview clusters windows by tag into separate regions instead of one mixed grid. One occupied tag fills the whole overview area; each additional occupied tag splits the most-recently-added region in half, cascading the same way the `dwindle` layout cascades a new window next to the focused one. Each window's card also gets a tag-colored border and a small tag-number badge (top-left corner) when this is enabled.
- `tagcolors` — Per-tag color palette used for the tag badge/border in grouped overview mode. Falls back to a built-in 8-color palette if unset.

[`overcircle`](/docs/bindings/keys#focus--movement) opens overview; while
overview is open, each trigger cycles focus to the next window on the current
monitor. Release a modifier key or run `toggleoverview` to close it.

By default overview temporarily views every tag on the monitor. Use
`overcircle` with `current_next`/`current_prev`, or run `toggleoverview,1`, to
keep the overview restricted to the current tagset's windows.

### Mouse Interaction in Overview

When in overview mode:

- **Left mouse button** — Jump to (focus) a window.
- **Right mouse button** — Close a window.
