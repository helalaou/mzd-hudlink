# Notice

HUDLink is an unofficial modification for Mazda Connect head units. It isn't
affiliated with, endorsed by or supported by Mazda Motor Corporation, Apple Inc.
or Google LLC. Mazda, Mazda Connect, CarPlay, Apple Maps and Google Maps are
trademarks of their respective owners.

## License

The whole project is licensed under the GNU Affero General Public License v3.0
or later (see [LICENSE](LICENSE)). Every source file states its license in an
`SPDX-License-Identifier` line. The AGPL comes from the code HUDLink is built
on (listed below), so it can't be swapped for a more permissive license.

## Copyright

**Copyright (C) 2026 KID MIXER-MODER** for the original CarPlay-to-HUD bridge,
[mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud), which
HUDLink was forked from at commit `9782e92`. This covers the bridge's design
and the code in `mazda/` as it was at that commit:

- the `jciCARPLAY` process check and the device manager hooks (`main.cpp`,
  `devmgr_shim.cpp`);
- decoding Apple's route guidance messages and choosing which maneuver to show
  (`nav.cpp`);
- sending to the HUD through the head unit's own `libjcidbus` and
  `libjcivbsnaviclient`, whose interfaces were worked out by reverse
  engineering (`oem/`, `patch.h`, `log.h`);
- the cross-compile `Makefile`.

**Copyright (C) 2026 The HUDLink authors** (see [AUTHORS](AUTHORS)) for
everything changed in those files since commit `9782e92`, and for everything
added since: the USB scripts (`usb/`), the build and test tools (`tools/`,
`tests/`) and the documentation.

## Adapted code

- **headunit** by Trevelopment and the headunit contributors,
  <https://github.com/Trevelopment/headunit>, AGPL-3.0. The HUD sender
  (`mazda/patches/blmjcicarplay/hud/hud_send.cpp` and `hud_send.h`) is adapted
  from headunit's HUD code, and the cross-compile setup follows headunit's
  approach. Apart from the original bridge, this is the only outside code in
  HUDLink. Its AGPL-3.0 license is why the bridge, and therefore HUDLink, is
  AGPL-3.0.

## Not included

- **m3-toolchain** by lmagder, <https://github.com/lmagder/m3-toolchain>: the
  ARM cross-compiler. The build container (`tools/Dockerfile`) downloads it at
  a pinned commit, but none of it is stored in this repository.
- **mzd-v74-tweaks** by piotrmocko,
  <https://github.com/piotrmocko/mzd-v74-tweaks>, CC BY-NC-SA 4.0: its release
  sets up the USB drive that starts HUDLink. None of its files are in this
  repository, and HUDLink's USB scripts were written from scratch for this
  project.
- **Mazda's files.** HUDLink doesn't ship any file from the head unit. The
  installer edits the head unit's own files in place, and the test fixtures in
  `tests/fixtures/` are small stand-ins shaped like the real files, written for
  the tests.
