# Changelog

Dated log of work on this fork. Format: date, what changed, why,
verification status.

---

## 2026-09-18 — Repo setup

**Context:** starting a fork to add an opt-in tag-grouped overview mode to
mango (stock overview mixes every tag's windows into one flat grid, with no
visual indication of workspace membership).

**Done:**
- Forked `mangowm/mango` to `FluxWav/mango`, cloned to `~/Projects/mango`.
  `origin` = fork, `upstream` = `mangowm/mango`.
- Branch `tag-grouped-overview` off `main` (tracking upstream main, not
  pinned to any release tag — this stays a personal build for now, but
  main is what a future PR would target).
- Verified the full build toolchain against this machine before writing any
  code: every dependency `meson.build` declares (`wlroots-0.20`,
  `scenefx-0.5`, `libdrm`, `xcb`/`xcb-icccm`/`xcb-randr`, `wayland-server`,
  `xkbcommon`, `libinput`, `wayland-client`, `libpcre2-8`, `pixman-1`,
  `libcjson`, `pangocairo`, `wayland-protocols`) resolves via `pkg-config`
  with versions satisfying every constraint mango declares — Arch/CachyOS
  already ships exact-version matches for `wlroots-0.20`/`scenefx-0.5` with
  dev headers, so no need to hand-build either from source. Only `meson`
  was missing.
- Confirmed nested-backend support directly from `src/main.c` (stock
  `wlr_backend_autocreate()` + `<wlr/backend/wayland.h>`), not just assumed.
- Confirmed SDDM's default `SessionDir` does **not** scan any per-user
  directory (`man sddm.conf`) — a future dev session entry has to go in
  `/usr/local/share/wayland-sessions/`, not `~/.local/share/`.

**Verification:** `git remote -v` shows both remotes correctly; `git log
--oneline -1` on the new branch matches upstream `main` HEAD at fork time.

**Fixed along the way:** first pass overwrote upstream's `README.md`
(which describes the whole mango project) with fork-specific content.
Restored it via `git checkout -- README.md` and moved the fork notes to a
separate `FORK.md` instead — keeps `README.md` untouched for merge-
friendliness if this ever goes upstream.

**Next:** install `meson`, do a first nested build of the *unmodified* tree
as an end-to-end build sanity check before writing any feature code.

## 2026-09-18 — First nested build (unmodified tree)

**Done:**
- Installed `meson` (`pkexec pacman -S meson` — no TTY available for a
  `sudo` prompt in this session; `pkexec` triggered the polkit GUI prompt
  instead).
- `meson setup build --prefix=$HOME/.local/mango-dev && ninja -C build` —
  clean build, every dependency found exactly as verified in the prior
  entry, no errors.
- Ran `./build/mango -d -c ./test-nested.conf` (a minimal throwaway config,
  `test-nested.conf`, gitignored — deliberately not the real desktop
  config, to avoid spawning a second noctalia instance against live state).
  Confirmed genuinely nested (not grabbing the real display) via the log
  line `backend/wayland/output.c:217] DMA-BUF imported into parent Wayland
  compositor`, and confirmed the instance was fully live via `mmsg` against
  its own socket (`mango-<pid>.sock`, separate from the real session's):
  `WL-1` output at 1862x2060, 4 tags, version `0.17.2(091dc44a)` matching
  this branch's HEAD commit exactly. Quit cleanly with `mmsg dispatch
  quit`.

**Verification:** IPC-level (`mmsg get version`, `get all-monitors`)
against the nested instance's own socket — not a screenshot.

**Next:** config parsing (`overview_group_by_tag`, `tagcolors`) in
`src/config/parse_config.c`.

## 2026-09-18 — Config parsing, per-card badge + border

**Done:**
- Config: `overview_group_by_tag` (bool) and `tagcolors` (comma list,
  `0xRRGGBBAA` format — matching mango's existing `bordercolor`-style
  convention, not CSS `#RRGGBB`) in `parse_config.h`/`.c`, following the
  same 5-touchpoint pattern as `enable_hotarea`/`jump_labels` (struct
  field, parser branch, clamp, default). Default palette of 8 colors,
  cycled if there are more tags than palette entries.
- **Border**: reading `get_border_color()` (`src/manage/client.c:1104`)
  showed mango already draws every window's border as a `wlr_scene_rect`
  colored by that function — no new scene node needed. Added a first
  branch returning `config.tag_colors[get_client_tag_idx(c)]` when
  `m->isoverview && config.overview_group_by_tag`. Refreshed via
  `client_update_border_color(c)` at the end of `overview_backup()` /
  `overview_restore()` (`src/overview/overview.c`).
- **Badge**: reading `include/mango/draw/text-node.h` showed mango already
  has a full labeled-chip primitive, `MangoJumpLabel`, used for the
  jump-mode keyboard hint (`Client.jump_label_node`). Added a second
  always-on instance (`Client.tag_label_node`) instead of a new node type
  — lazily created in `src/layout/arrange.c` (mirrors how
  `jump_label_node` itself is created), colored via
  `mango_jump_label_node_set_background()`/`set_border()` from
  `config.tag_colors`, positioned top-left-corner (vs. jump label's
  centered) in a new `overview_update_tag_badge()`
  (`src/overview/overview.c`), called from the same `overview_layout_card()`
  call site as the jump label. Explicitly disabled in `overview_restore()`
  when leaving overview (mirrors `finish_jump_mode()`'s jump-label
  disable) — without this it stays visible/stale after overview closes.
  Destroyed alongside `jump_label_node` at client teardown.
- Revised `DESIGN.md` to match — both pieces turned out simpler than the
  original plan (reuse existing infra instead of new scene-rect/node
  types) once the actual code was read.

**Verification:** `ninja -C build` clean after each piece. Nested-tested
with two real ghostty windows split across tags 1/2 (`test-nested.conf`,
now includes `overview_group_by_tag = 1` and a `tagcolors` line): spawned
both, triggered `toggleoverview`, confirmed via `mmsg get all-clients` both
become `is_visible: true` and no crash/error in the debug log. Actual pixel
correctness (badge text/position, border color) not yet visually confirmed
— needs a real look, not just IPC state.

**Next:** the spatial grouping/partition algorithm
(`overview_scale_grouped()`), the riskiest and largest remaining piece.
