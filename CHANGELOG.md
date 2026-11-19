# Changelog

## 1.0.0 (unreleased)

The first HUDLink release, forked from
[mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud) at commit
`9782e92` and tested in a car over wireless CarPlay with Apple Maps and Google
Maps.

### Added

- **USB installer** (`usb/hudlink.sh`) with on-screen Install and Uninstall
  buttons. It only edits the `jciCARPLAY` service entry, tests the bridge on the
  head unit before anything points to it, writes through checked temporary
  copies, restores the factory files if a write fails, and on uninstall removes
  exactly what it added.
- **USB launcher** (`usb/launcher.sh`) and a **Save logs** step
  (`usb/hudlink-logs.sh`) that copies everything a bug report needs to the
  drive.
- **Feet and miles** on the HUD (`UNITS=imperial`).
- **Off switch:** if the CarPlay process crashes, the bridge stays off until
  Install is run again.
- **Diagnostic build** (`make diag`) that keeps a detailed log on the head unit.
- **Tooling:** a pinned build container (`tools/build.sh`), a script that
  assembles the USB files (`tools/make-usb.sh`), and tests that run the USB
  scripts on a fake head unit (`tools/test.sh`).

### Changed

- The CarPlay service keeps its factory `LD_LIBRARY_PATH`.
- All log lines go to one size-capped log file, and guidance request retries
  are only logged when the result changes.
- The HUD sender builds and sends each frame in a single path and clears the
  HUD with one request.
- Builds are reproducible: the bridge sources are linked in a fixed order, and
  objects are rebuilt whenever the build options change.
- The toolchain path is set on the `make` command line instead of through a git
  submodule.
- Every bridge source file was cleaned up, and the comments were rewritten to
  describe the code as it actually is.

### Removed

- The original install scripts (`install/`), replaced by `usb/`.
- The compiled-out speed limit code, the reverse-engineering switches and an
  unused copy of headunit's D-Bus proxies.
