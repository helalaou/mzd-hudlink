# Building HUDLink

`tools/make-usb.sh` builds the bridge automatically when it makes the USB files
(see the [install guide](install.md)), so this page is only needed for working
on the code.

## Building the bridge

```bash
tools/build.sh UNITS=imperial
```

The bridge is written to `mazda/build/release/libpatch-blmjcicarplay.so`.
Builds are reproducible: in the pinned build container, the same source and
options always produce the same bytes.

| Option | What it does |
|---|---|
| `UNITS=metric` | meters and kilometers (the default) |
| `UNITS=imperial` | feet and miles |
| `VIETNAMESE=1` | replaces Vietnamese letters the HUD's font can't display |
| `diag` | builds the diagnostic version, which keeps a detailed log on the head unit |
| `debug` | builds an unoptimized version that logs everything |

Options can be combined, for example `tools/build.sh diag UNITS=imperial`. To
put the diagnostic version on a USB drive, run
`tools/make-usb.sh --units imperial --diag`.

Without Docker, `make` also runs directly on Linux x86-64 with a checkout of
[lmagder/m3-toolchain](https://github.com/lmagder/m3-toolchain) at commit
`61ec0343`:

```bash
make -C mazda M3TOOLCHAIN=/path/to/m3-toolchain UNITS=imperial
```

## Tests

```bash
tools/test.sh
```

This runs the USB scripts against a fake head unit using BusyBox, the same
shell and tools the real head unit has. The tests cover each refusal, install,
update, uninstall back to the original bytes, writes that fail halfway, the off
switch, Save logs and its backup, and the launcher.

To also test against your own car's files, add `--factory DIR` with copies of
your head unit's `sm.conf` and `sm_WCP.conf` (details in
[tests/run.sh](../tests/run.sh)). Mazda's files are not included in this
repository.

## Repository layout

```
mazda/
  Makefile                   cross-compiles the bridge
  patches/blmjcicarplay/
    main.cpp                 entry point, only active in jciCARPLAY; the off switch
    devmgr_shim.cpp          device manager hooks: requests route guidance, reads it
    nav.cpp                  decodes route guidance, picks the maneuver to show
    hud/hud_send.cpp, .h     builds and sends HUD frames (adapted from headunit)
    oem/                     declarations for the head unit's own D-Bus and HUD calls
    log.h, patch.h           logging and shared declarations
usb/
  launcher.sh                the menu the USB drive opens
  hudlink.sh                 the installer
  hudlink-logs.sh            the Save logs step and the backup
tools/
  Dockerfile, build.sh       the pinned build container and the build script
  make-usb.sh                assembles the USB files in dist/usb/
  test.sh                    runs the tests in a container
tests/                       the fake head unit tests and their fixtures
docs/                        the install guide, background, internals and this page
```

[How HUDLink works](how-it-works.md) describes what each part of the bridge
does and how the installer checks the head unit.
