# lumen-filemanager

The graphical file manager for **AspisOS**, a capability-based,
no-ambient-authority operating system built on the from-scratch
[Aegis](https://github.com/AspisOS/Aegis) kernel.

lumen-filemanager is a traditional file manager — a column list view with
navigation history, file operations, drag-and-drop, and "open with" dispatch.
It is an external client of the [lumen](https://github.com/AspisOS/lumen)
compositor (the same window-protocol pattern as the terminal and editor),
distributed as a [herald](https://github.com/AspisOS/AspisOS) package and
installed as an `/apps` bundle. Its descriptor's display name is **Files**.

## The AspisOS ecosystem

AspisOS is decomposed into independent repositories; lumen-filemanager is one
graphical leaf of that tree:

| Repo | Role |
|------|------|
| [`AspisOS/Aegis`](https://github.com/AspisOS/Aegis) | The kernel. Provides the capability model, `AF_UNIX` sockets, the filesystems the manager browses, and the `SYS_SPAWN` syscall it uses to open files. |
| [`AspisOS/lumen`](https://github.com/AspisOS/lumen) | The compositor / display server. Every GUI app is a lumen client; the file manager connects to its socket for a window, input events, and drag-and-drop brokering. |
| [`AspisOS/glyph`](https://github.com/AspisOS/glyph) | The GUI toolkit. Supplies the software renderer (`draw_*`, `font_*`), the theme palette, and the client side of lumen's window protocol (`lumen_client.h`) this app links against. |
| [`AspisOS/AspisOS`](https://github.com/AspisOS/AspisOS) | The OS: userland, rootfs, ISO/installer, and the herald package manager that installs this `.hpkg`. |

## What it does

Grounded in `src/main.c`:

- **One window.** Connects to lumen and draws a single 640×480 window (clamped
  to fit small framebuffers via the `LUMEN_FB_W` / `LUMEN_FB_H` hints). It opens
  at `argv[1]` if given, otherwise `$HOME`, otherwise `/`.
- **List view.** A two-column list (Name | Size), folders first
  (case-insensitive sort), with alternating row stripes, human-readable sizes,
  a draggable scrollbar, a toolbar, and a status line. The toolbar holds
  back / forward / up navigation, a tail-truncated path display, and
  New Folder / Rename / Delete buttons; navigation keeps a back/forward history
  stack.
- **File operations.** `mkdir`, `rename` (collision-checked), delete via a
  confirm dialog (`unlink` / `rmdir`), and copy / cut / paste through an
  internal clipboard. A cross-filesystem move (e.g. `/tmp` ↔ ext2, where
  `rename` fails with `EXDEV`) falls back to a chunked copy + unlink.
- **Context menu & "open with".** Right-click opens a context menu (row or
  background). For files, an "Open with…" item drills the panel into a list of
  openers — Editor, Tunes, Image Viewer — with a "set as default" checkbox that
  persists an extension→app association to `$HOME/.openwith` (one `ext app`
  line per type).
- **Drag-and-drop.** Dragging a row past a small slop hands the gesture to the
  compositor (`lumen_drag_start`), which draws the ghost and brokers
  `DRAG_OVER` / `DRAG_LEAVE` / `DROP` events back; dropping onto a folder (or
  the "up" button) moves or copies the file. The same drop path accepts files
  dragged from other apps.
- **Opening.** Double-click or Enter descends into directories in place. For
  files, a user-set `.openwith` default wins; otherwise dispatch is by
  extension — images (`.png/.bmp/.jpg/.jpeg`) → `/apps/imageviewer/imageviewer`,
  audio (`.wav/.mp3`) → `/apps/tunes/tunes`, everything else → the text
  `/apps/editor/editor` (files over 512 KB are refused). Each opener is spawned
  as a sibling lumen client via `SYS_SPAWN` (syscall 514), with `stderr` routed
  to `/dev/console`.
- **Keys.** Arrows move, Enter opens, Backspace goes up, Esc closes (or cancels
  a dialog), `^N` new folder, `^R` rename, `^D` delete, `^C` copy, `^X` cut,
  `^V` paste, `m` opens the context menu, `.` toggles hidden (dot) files.
  (Delete and F2 are E0 scancodes the PS/2 path does not deliver yet, hence the
  control-key bindings.)

## Capabilities

AspisOS has no ambient authority: a process can do nothing except through
capabilities granted at exec time. lumen-filemanager's policy
(`pkg/etc/aegis/caps.d/filemanager`) is the baseline desktop-app profile:

```
service
```

It carries no elevated capabilities of its own. Directory listing, the file
operations, and spawning the sibling opener apps all run under the `service`
profile granted to a Lumen client — there is no setuid, no power, no special
device access.

Because its herald package id (`lumen-filemanager`) differs from the bundle/exec
name (`filemanager`) and it installs a binary plus a cap policy and an app
descriptor across `/apps` and `/etc`, it is a `class=system` package:
first-party and signature-trusted, installed verbatim by herald.

## Building

lumen-filemanager fetches a pinned [glyph](https://github.com/AspisOS/glyph)
toolkit artifact (the GUI libraries it links) and builds against it, then packs
a signed herald package.

```sh
make MUSL_CC=/path/to/musl-gcc HERALD_KEY=/path/to/signing.key
```

- `GLYPH_VERSION` pins the toolkit release fetched by `tools/fetch-glyph.sh`.
- `MUSL_CC` is the musl cross-compiler (the only toolchain assumption — point it
  at an Aegis-native `cc` to build on-device in the future).
- `HERALD_KEY` signs the `.hpkg` (ECDSA P-256).

Output: `lumen-filemanager.hpkg` (a `class=system` herald package) +
`lumen-filemanager.hpkg.sig`.

## Package payload

The `.hpkg` is a manifest-first, uncompressed POSIX `ustar` archive with a
detached signature. Its payload tree:

```
/apps/filemanager/filemanager        the app binary (stripped)
/apps/filemanager/app.ini            the bundle descriptor (name=Files, exec=filemanager)
/etc/aegis/caps.d/filemanager        its capability policy
```

## Repository layout

```
src/        filemanager source
pkg/        install-tree skeleton shipped verbatim (app.ini + caps.d)
tools/      fetch-glyph.sh (toolkit fetch) + pack.sh (build the signed .hpkg)
Makefile    fetch toolkit -> build -> pack
VERSION         this component's version
GLYPH_VERSION   the pinned glyph toolkit version it builds against
```

## Dependencies

`depends=lumen` — the file manager is a Lumen client and opens files into Lumen
apps, so installing it pulls [lumen](https://github.com/AspisOS/lumen) (which in
turn ships the desktop fonts every dependent inherits). The opener apps it
dispatches to ([editor](https://github.com/AspisOS/lumen-editor),
[imageviewer](https://github.com/AspisOS/lumen-imageviewer),
[tunes](https://github.com/AspisOS/lumen-tunes)) are resolved at runtime by path
and used only if present.

## Status

Functional and used as the desktop's primary browser, with a few honest gaps:
copying or pasting whole *folders* is not supported yet (single-file copy and
folder *moves* on the same filesystem work); the text editor refuses files over
512 KB; and the Delete / F2 keys are unbound pending E0 scancode delivery from
the PS/2 path. Expect these to close as AspisOS matures.
