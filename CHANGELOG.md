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

## 2026-09-18 — v2 grouping algorithm: grid partition + real per-tag layout reuse

**Done:**
- User shared real mockups of the intended layout — a clean grid-of-rows
  region partition (matching mango's own `grid()` layout's row/column math
  almost exactly for N=2/3/4), each region showing that tag's *actual*
  configured layout, not a generic packer. Superseded the dwindle-cascade
  v1 entirely rather than keeping it as a fallback mode.
- Deep research pass (before writing any code) into whether "temporarily
  redirect `m->w`/current-tag, call the tag's real `Layout->arrange(m)`
  unmodified, restore after" is actually safe — found and pre-solved a
  real gap: `grid()`/`dwindle()` trust monitor-global
  `visible_*_tiling_clients` counters rather than recomputing them, so the
  redirect needs `pre_calculate_before_arrange(m, false, false, true)`
  (the existing counting function, `only_calculate=true`) before *and*
  after the per-tag loop, not just the `m->w`/tag swap alone.
- Extracted `compute_grid_dims()` out of `grid()` (`src/layout/
  horizontal.c`) — pure refactor, `grid()` itself unchanged in behavior.
  Regression-verified: 5 windows in a real `grid`-layout tag arrange
  identically before/after (3-over-2, short row centered, matches
  pre-extraction output exactly).
- Wrote the v2 `overview_scale_grouped()`: grid-of-rows partition via
  `compute_grid_dims()`, then per occupied tag save/redirect (`m->w`,
  `tagset[seltags]`, `pertag->curtag`)/recount/`Layout->arrange(m)`/restore.

**Bugs found and fixed during nested testing (not assumed correct from
review alone):**
- SIGSEGV on first overview-open with all 4 tags occupied. Root-caused
  with `coredumpctl` + `gdb bt`: `get_client_tag_idx()` already returns
  the 1-based tag number (matching `pertag->curtag`/`ltidxs[]`'s own
  convention), not a 0-based bit position as assumed — an extra `+1` in
  both the new grouping code *and* the already-committed badge code
  walked one slot past a real tag, landing on `ltidxs[]`'s uninitialized
  (NULL) top slot. Fixed both; confirmed by tracing every other real call
  site of `get_client_tag_idx()` in the codebase, not just this one.
- Scroller layout produced off-region (even off-monitor) x positions for
  non-focused stack windows in a small region — traced to its
  non-centered path anchoring on the root client's *stale* full-width
  `geom.x`. Fixed the root's own positioning by temporarily forcing
  `config.scroller_focus_center = 1` around the per-tag loop (save/
  restore, reuses scroller()'s existing centered-mode path, zero new
  logic). Multi-window scroller tags can still scroll a window off-region
  in a small enough space — confirmed this matches scroller()'s real
  behavior on a narrow real monitor too (not new corruption), flagged for
  the user rather than papered over with more custom code.

**Verification:** nested, 4 real tags (scroller/dwindle/tile/grid, 2/2/1/5
windows) — no crash, `grid`-layout regression check passed, dwindle/tile/
grid regions all correctly rescale and reflow, scroller's root window
correctly centered. Scroller's second window overflow is a known,
understood, flagged-not-fixed edge case. Confirmed no state corruption on
the real tag after closing grouped overview (checked scroller geometry
before/after matches).

**Next:** decide with the user how to handle the scroller-overflow edge
case, then a real visual pass on the nested window.

## 2026-09-18 — Grouping/partition algorithm

**Done:**
- Extracted `overview_pack_region(Client **client_list, int n, struct
  wlr_box region, int32_t gap_inner)` out of `overview_scale()` — same
  qsort/`try_place()`/binary-search-on-scale/`center_placed_rows()`/
  `client_tile_resize()` logic, generalized to an arbitrary target box and
  client subset instead of hardcoded to the whole monitor and all clients.
  `overview_scale()` is now a thin wrapper: builds the client list, computes
  the monitor's gappo-inset box, calls the shared helper once.
- New `overview_scale_grouped(Monitor *m)`: buckets visible clients by
  `get_client_tag_idx()`, builds the ascending occupied-tags list, cascades
  the outer box (most-recently-added region splits in half per new
  occupied tag, axis via the same `width >= height` rule `dwindle_assign`
  uses), packs each region via the shared helper. Wired into `overview()`:
  `config.overview_group_by_tag` picks this path over the flat one (the
  `ov_tab_layout`/overcircle branch is unaffected, checked first).

**Verification (nested, structural via `mmsg`, not yet visual):**
- 5 real ghostty windows across 3 tags (2/1/2 split), `overview_group_by_tag
  = 1`: triggered `toggleoverview`, no crash, and the resulting geometry
  matched the intended cascade exactly — tag 1 top half full-width, tag 2
  bottom-left quadrant, tag 3 bottom-right quadrant, zero region overlap,
  clean 30px gaps matching `overviewgappo`.
- Same window layout with `overview_group_by_tag = 0`: both windows from
  different tags landed back in one flat packed grid together, confirming
  the refactor didn't change existing flat-mode behavior.

**Not yet done:** 4-occupied-tag case untested; real pixel appearance
(badge text/position, border color) unconfirmed — IPC geometry checks
can't see color or text rendering, need an actual look at the nested
window.

**Next:** a real visual pass (someone actually looking at the nested
window), then decide on daily-use packaging (SDDM session entry).
