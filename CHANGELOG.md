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
