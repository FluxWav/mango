# This fork

Adds an optional tag-grouped mode to the window overview (`toggleoverview`).

Stock mango's overview shows every window from every tag mixed into one
flat grid, with no indication of which workspace a window belongs to. This
fork adds:

- A tag-number badge and a tag-colored border on every window card.
- An opt-in grouped layout (`overview_group_by_tag`) that clusters windows
  by tag into separate cascading regions, instead of one undifferentiated
  grid.

See `DESIGN.md` (untracked, local only) for the full design writeup, and
`CHANGELOG.md` for the dated work log.

## Building

Dependencies match upstream mango exactly — see upstream's
[installation guide](https://mangowm.github.io/docs/installation). On
Arch/CachyOS, `wlroots-0.20` and `scenefx-0.5` are already packaged with
dev headers, so only `meson` needs installing:

```sh
sudo pacman -S meson   # if not already installed
meson setup build --prefix=$HOME/.local/mango-dev
ninja -C build
```

Do not `ninja install` to `/usr` — this keeps the build out of the way of
the system-packaged `mango` binary entirely.

## Running

Test nested, inside an existing Wayland session, before anything else:

```sh
./build/mango
```

wlroots auto-detects the existing session and opens a nested window instead
of grabbing the real display.

## Enabling the feature

```
overview_group_by_tag = 1
tagcolors = #47add6,#b153a7,#14a57c,#ad401f
```

in your mango config. Off by default — stock behavior is unchanged when
unset.
