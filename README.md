# lumen-filemanager

The graphical file manager for **AspisOS**, a capability-based,
no-ambient-authority operating system built on the from-scratch
[Aegis](https://github.com/AspisOS/Aegis) kernel.

lumen-filemanager is a traditional file manager that speaks the
[lumen](https://github.com/AspisOS/lumen) external-window protocol (the same
pattern as the terminal and editor). It is a standalone component of the Lumen
desktop, distributed as a [herald](https://github.com/AspisOS/AspisOS) package
and installed as an `/apps` bundle.

## Role in the system

- An ordinary Lumen client: it connects to the compositor over its socket and
  draws a single 640x480 window. It is launched from the desktop like any other
  `/apps` bundle (its descriptor's display name is **Files**).
- Column list view (Name | Size), folders first, with a scrollbar, a path/back/
  forward/up toolbar, and a status line.
- File operations: `mkdir`, `rename`, `unlink`/`rmdir` (with a confirm dialog),
  and copy / cut / paste via an internal clipboard. A cross-filesystem paste
  (e.g. `/tmp` ↔ ext2) falls back from `rename` to copy + unlink.
- Opening a file spawns a sibling Lumen client via `SYS_SPAWN`: directories are
  descended in place, while files are dispatched by extension to a helper app —
  `/apps/editor/editor`, `/apps/imageviewer/imageviewer`, or
  `/apps/tunes/tunes`. Per-user "open with" defaults are stored at
  `$HOME/.openwith` (one `ext app` line per type).
- Keys: arrows move, Enter opens, Backspace goes up, Esc closes/cancels a dialog,
  `^N` new folder, `^R` rename, `^D` delete, `^C` copy, `^X` cut, `^V` paste.

## Capabilities

lumen-filemanager's cap policy (`pkg/etc/aegis/caps.d/filemanager`) is the
baseline desktop-app profile:

```
service
```

It carries no elevated capabilities of its own — directory listing, the file
operations, and spawning helper apps all run under the `service` profile granted
to a Lumen client.

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
- `HERALD_KEY` signs the `.hpkg`.

Output: `lumen-filemanager.hpkg` (a `class=system` herald package) +
`lumen-filemanager.hpkg.sig`.

## Package payload

```
/apps/filemanager/filemanager        the app binary
/apps/filemanager/app.ini            the bundle descriptor
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
turn provides the desktop fonts). The opener apps it dispatches to
([editor](https://github.com/AspisOS/lumen-editor),
[imageviewer](https://github.com/AspisOS/lumen-imageviewer),
[tunes](https://github.com/AspisOS/lumen-tunes)) are resolved at runtime if
present.
