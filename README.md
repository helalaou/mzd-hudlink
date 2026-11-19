# Mazda HUDLink

**View Your CarPlay directions, on your Mazda's head-up display.** HUDLink bridges
Apple CarPlay, wired or wireless, to the factory head-up display of
first-generation Mazda Connect cars. Apple Maps and Google Maps directions
appear on the HUD the same way the built-in SD-card navigation's directions do,
and the HUD's speed and traffic sign display keeps working.

## Background

### CarPlay in Mazdas

Mazda added Apple CarPlay to its lineup over several years. According to Mazda
dealer information, these models came with CarPlay from the factory in the
following model years (years can differ by market):

| Model | Factory CarPlay |
|---|---|
| Mazda2 | 2020–2024 |
| Mazda3 | 2019–2024 |
| Mazda6 | 2018–2022 |
| CX-3 | 2020–2024 |
| CX-30 | 2020–2024 |
| CX-5 | 2019–2024 |
| CX-50 | 2022–2024 |
| CX-60 | 2022–2024 |
| CX-70 | 2025 |
| CX-8 | 2019–2024 |
| CX-9 | 2019–2024 |
| CX-90 | 2024 |
| MX-30 | 2021–2024 |
| MX-5 Miata | 2019–2024 |

A Mazda outside this list that has CarPlay most likely had it added later. In
late 2018, Mazda started offering an official upgrade for 2014-and-newer cars
with Mazda Connect: a dealer installs the latest Mazda Connect firmware and new
USB ports, for $199 plus labor
([Car and Driver](https://www.caranddriver.com/news/a25250932/mazda-apple-carplay-android-auto-upgrade/)).
The install typically comes to around $600 out the door, and it only adds
CarPlay to the center screen, not to the HUD. Other cars got CarPlay from a
third-party installer, or from the owner with an aftermarket adapter. Setups
vary as a result: the official upgrade runs CarPlay over a USB cable, while
wireless CarPlay on these older cars comes from aftermarket adapters and hubs.

### CarPlay and the head-up display

None of these options connect CarPlay to the head-up display (Mazda calls it
the Active Driving Display). Mazda began showing CarPlay directions on the HUD
around the 2023–2024 model years, depending on the model and region. Earlier
Mazda Connect cars, roughly the 2014 to 2022–2023 models, only show directions
from the built-in navigation on the HUD. Even on the newer cars, owners report
that HUD directions work most reliably with Apple Maps, and that support for
Google Maps and Waze has varied with the model year, the car's software, and
iOS and app updates (see the threads on the
[CX-90 Forum](https://www.cx90forum.com/threads/google-maps-on-hud.2299/),
[r/CX5](https://www.reddit.com/r/CX5/comments/1lbisac/turn_by_turn_navigation_in_hud/)
and [Mazdas247](https://mazdas247.com/forum/threads/hud-directions-on-a-pre-2024-cx-5.123881937/)).

On cars with the first-generation Mazda Connect head unit (CMU150), the HUD
only shows directions from the built-in SD-card navigation. This is the case
whether CarPlay came from the factory or was added later, and whether or not
the parts are genuine: CarPlay runs on the center screen, its directions never
reach the HUD, and no firmware update for these cars changes that. A dealer
can't fix it either. The official upgrade costs around $600 and adds CarPlay to
the screen but not to the HUD, and Mazda offers no update or part that adds HUD
directions the way the newest models have them. As a result, the HUD shows no
directions while you navigate with CarPlay.

### What HUDLink does

On a CMU150 head unit with firmware 74.00.324, HUDLink shows live turn-by-turn
directions from Apple Maps and Google Maps on the HUD, with wired or wireless
CarPlay, regardless of how CarPlay was added to the car. The only requirement
is that CarPlay runs through the head unit itself, as it does with factory
CarPlay, Mazda's official upgrade, or a wireless adapter with smartBox
firmware. CarPlay keeps running on the center screen as usual. Cars with the
second-generation Mazda Connect, introduced with the 2019 Mazda3, use a
different head unit and are not supported.

Nothing needs to be installed on the phone, and no internet connection is
needed. HUDLink reads the route guidance the iPhone already sends to the car.

## Features

HUDLink sends directions to the HUD through the same head unit calls the
built-in SD-card navigation uses, so CarPlay directions look and behave like
the factory ones:

- **Live turn-by-turn directions.** The distance to the next turn counts down
  as you drive, and the HUD updates whenever the route is recalculated.
- **Turn icons** for left and right turns, sharp and slight turns, forks and
  keep left/right, highway changes, on and off ramps, U-turns, roundabouts with
  the exit to take, ferries, and the destination.
- **Road names** for the road you're turning onto.
- **Speed and traffic signs.** The HUD's own speed readout and traffic sign
  display keep working next to the directions, as they do with the built-in
  navigation.
- **Wired or wireless CarPlay.** The bridge runs inside the head unit's CarPlay
  process, so it works the same way whether the iPhone is connected by cable
  or through a wireless adapter with smartBox firmware.
- **Clears when guidance ends.** The directions disappear when the route ends
  or CarPlay disconnects, and the built-in SD-card navigation works as before.

## About this fork

HUDLink is a fork of
[mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud) (AGPL-3.0)
at commit `9782e92`. Changes since the fork:

- **Got it working over wireless CarPlay.** Built a diagnostic version that
  logs what happens inside the head unit, used it to find out why the HUD
  stayed blank, and tested on the road until Apple Maps and Google Maps
  directions showed up correctly.
- **Rewrote the installer from scratch.** It tests the bridge on the head unit
  before changing anything, edits only the `jciCARPLAY` service entry and the
  `NaviSupported` setting, keeps CarPlay's factory library path, and restores
  every file byte for byte on uninstall. It ships no Mazda files.
- **Cleaned up the bridge.** Reworked every source file: removed dead code,
  simplified the structure, rewrote the comments to match the code, and cut
  about 350 lines.
- **Added** feet and miles (`UNITS=imperial`), an off switch that disables the
  bridge after a crash, a Save logs step that also backs up the files Install
  changes, and leaner logging.
- **Added build and test tools:** reproducible builds in a pinned container, a
  script that packages the USB drive, and tests that run the installer on a
  fake head unit.

The full list of changes is in [CHANGELOG.md](CHANGELOG.md).

## Tested

| | |
|---|---|
| Head unit | Mazda Connect CMU150, firmware 74.00.324A, North America |
| CarPlay | Wireless, through a third-party hub with smartBox firmware (Carabc "smartbox bfcf") |
| Apps | Apple Maps and Google Maps |
| On the HUD | Turn arrows, distance to the turn and road names, alongside the HUD's own speed and traffic sign display |

HUDLink has been tested in one car so far, with wireless CarPlay. Wired CarPlay
reaches the bridge through the same path inside the head unit. The original
bridge was developed on the Asian (ADR) version of firmware 74.00.324, and the
test car runs the North American one. Other cars on firmware 74.00.324 should
behave the same way, and the installer checks each head unit before it changes
anything.

## How to run it

### What you need

- A Mazda with the first-generation Mazda Connect head unit (CMU150) on
  firmware 74.00.324, and a head-up display. The firmware version is under
  Settings > System > About > Version Information. The second-generation Mazda
  Connect is not supported.
- An iPhone with CarPlay, connected by cable or through a wireless adapter with
  smartBox firmware (see [Known issues](#known-issues)).
- A computer with git and Docker: Docker Engine on Linux, or
  [Colima](https://github.com/abiosoft/colima) or Docker Desktop on macOS.
- A USB drive formatted as FAT32, 32 GB or smaller.
- If the original mazda-carplay-hud is installed, uninstall it first with its
  own uninstaller.

### 1. Build the USB files

Start Docker (with Colima, run `colima start`), then run this in a terminal at
the root of this repository:

```bash
tools/make-usb.sh --units imperial
```

Use `--units metric` instead for meters and kilometers. The first run sets up
the build container and downloads the ARM cross-compiler, so it takes a while;
later runs are much faster. The output goes to `dist/usb/`.

### 2. Set up the USB drive

1. Download `mzd-v74-tweaks-1.0.1.zip` from the
   [mzd-v74-tweaks releases](https://github.com/piotrmocko/mzd-v74-tweaks/releases)
   and unzip it to the root of the drive. It provides the file that makes the
   head unit run the drive's `run.sh`.
2. Copy everything from `dist/usb/` to the root of the drive, replacing
   `launcher.sh` and `launcher.sh.md5`.
3. On a Mac, delete the hidden `._` files macOS leaves on FAT32 drives (replace
   `MYDRIVE` with your drive's name):

   ```bash
   find /Volumes/MYDRIVE -name '._*' -delete
   ```

   A leftover `._` copy of the trigger file can make the head unit start the
   drive twice.

### 3. Back up the files Install changes

Install edits three existing files on the head unit and adds the bridge (see
the table at the end of this section), and Uninstall restores the three files
byte for byte. To keep your own copy of the originals as well, save them to the
drive before you install:

1. Start the car (engine on or accessory mode) and wait for the head unit to
   finish starting up.
2. Plug in the drive. After a few seconds, the HUDLink menu appears.
3. At **STEP 1 of 2: HUDLink**, press **Skip**.
4. At **STEP 2 of 2: Save logs**, press **Open**, then **Save**.
5. When it says **Done. Nothing needs a reboot**, pull out the drive.

On your computer, copy the `logs/check-<date>-<time>/backup/` folder from the
drive to a safe place. It holds `sm.conf`, `sm_WCP.conf` and
`devmgr_config_master.xml` exactly as they were on the head unit, and
`md5.txt` with their md5 sums. Save logs only reads from the head unit, so this
step changes nothing in the car.

### 4. Install in the car

1. Start the car (engine on or accessory mode) and wait for the head unit to
   finish starting up.
2. Plug in the drive. After a few seconds, the HUDLink menu appears.
3. At **STEP 1 of 2: HUDLink**, press **Open**, then **Install**. The installer
   tests the bridge on your head unit before it changes anything.
4. When it says **HUDLink is installed**, press **OK**, then press **Skip** at
   STEP 2.
5. Press **Reboot now**, and pull out the drive while the screen is black.

### 5. Drive

Start a route in Apple Maps or Google Maps. The arrow, the distance and the road
name appear on the HUD as soon as guidance starts, and the HUD clears when it
ends.

To uninstall, run the drive again and choose **Uninstall**. The
[install guide](docs/install.md) covers updates, every message the installer
can show, and what to try if the HUD stays blank.

Install changes these files, and Uninstall restores them:

| File | Change |
|---|---|
| `/data_persist/cp-hud-mod/libpatch-blmjcicarplay.so` | the bridge (a new file) |
| `/jci/sm/sm.conf`, `/jci/sm/sm_WCP.conf` | only the `jciCARPLAY` service entry: it loads the bridge, keeps CarPlay's factory library path, and restarts just CarPlay, not the whole head unit, if CarPlay fails (`reset_board="no"`) |
| `/etc/devmgr_config_master.xml` | `NaviSupported` goes from `FALSE` to `TRUE` |

## How it works

```
iPhone (CarPlay, navigating)
  |  route guidance (iAP2), over a cable or a wireless adapter
  v
device manager -> message queue
  |
  v
jciCARPLAY, the head unit's CarPlay process, with HUDLink loaded into it:
  |  asks the iPhone for route guidance once a CarPlay session starts,
  |  reads each guidance message as it arrives (without changing it),
  |  decodes the maneuver, the distance and the road name
  v
com.jci.vbs.navi, the head unit's HUD service (the same calls the built-in navigation makes)
  |
  v
Active Driving Display: arrow, distance, road name
```

Besides loading the bridge, the installer turns on `NaviSupported` in the
device manager's settings. That setting tells the iPhone the car can accept
route guidance. The message formats and the installer's checks are described
in [docs/how-it-works.md](docs/how-it-works.md).

## Building from source

`tools/make-usb.sh` builds the bridge automatically, so this section is only
needed for working on the code.

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

### Tests

```bash
tools/test.sh
```

This runs the USB scripts against a fake head unit using BusyBox, the same
shell and tools the real head unit has. The tests cover each refusal, install,
update, uninstall back to the original bytes, writes that fail halfway, the off
switch, Save logs and the launcher.

To also test against your own car's files, add `--factory DIR` with copies of
your head unit's `sm.conf` and `sm_WCP.conf` (details in
[tests/run.sh](tests/run.sh)). Mazda's files are not included in this
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
docs/
  install.md                 the install guide
  how-it-works.md            the data path, the message formats and the installer
```

## Known issues

- **Waze doesn't work.** It never sends route guidance to the car, so the HUD
  stays blank. If Waze is left open in the background, it can also keep the
  last turn on the HUD after Apple Maps or Google Maps has finished guiding.
  Closing Waze clears it
  ([mazda-carplay-hud#10](https://github.com/KidMixer/mazda-carplay-hud/issues/10)).
- **Some wireless adapters block route guidance.** A Carlinkit CPC200-U2W
  leaves the HUD blank, while adapters with smartBox firmware, like the hub in
  the test car, work
  ([mazda-carplay-hud#2](https://github.com/KidMixer/mazda-carplay-hud/issues/2)).
- **CarPlay guidance and built-in navigation guidance at the same time** hasn't
  been tested. The original bridge was developed without a navigation SD card,
  and its author warns that using one at the same time may cause conflicts
  ([forum post](https://www.mazda3revolution.com/threads/turn-by-turn-in-hud-with-carplay.258547/)).
- **Only firmware 74.00.324 is supported.** The installer refuses anything else.

### The off switch

If CarPlay crashes while the bridge is running, the bridge saves the crash
details on the head unit and stays off until Install is run again, so a fault
can't repeat on every drive. CarPlay keeps working; only the directions on the
HUD are lost. Running Install again turns the bridge back on and copies the
crash details to the USB drive for a bug report.

This happened once during testing, with an earlier build: CarPlay aborted,
possibly while shutting down. The off switch disabled the bridge and CarPlay
kept running. The cause wasn't found, and the build tested after it ran without
problems.

## Credits

HUDLink builds on these projects:

- **KID MIXER-MODER**,
  [mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud) (AGPL-3.0).
  The CarPlay-to-HUD bridge HUDLink is forked from, including the decoding of
  Apple's route guidance messages, the hooks inside `jciCARPLAY` and the HUD
  transport through the head unit's own libraries.
- **Trevelopment and the headunit contributors**,
  [headunit](https://github.com/Trevelopment/headunit) (AGPL-3.0). The HUD
  sender (`hud/hud_send.*`) is adapted from headunit's HUD code, by way of
  mazda-carplay-hud.
- **lmagder**, [m3-toolchain](https://github.com/lmagder/m3-toolchain). The ARM
  cross-compiler used to build the bridge. The build container downloads it;
  it isn't part of this repository.
- **piotrmocko**, [mzd-v74-tweaks](https://github.com/piotrmocko/mzd-v74-tweaks)
  (CC BY-NC-SA 4.0). Its release sets up the USB drive that starts HUDLink.
  None of its files are included here, and HUDLink's USB scripts were written
  from scratch.

Findings from the original project's issues also helped: ibm0078 and AmbrosiaAS
worked out which wireless adapters pass route guidance through
([#2](https://github.com/KidMixer/mazda-carplay-hud/issues/2)), SlendyMilky
found out why Waze leaves the HUD blank
([#10](https://github.com/KidMixer/mazda-carplay-hud/issues/10)), and ScorpAL's
review of the original installer
([#6](https://github.com/KidMixer/mazda-carplay-hud/issues/6)) pointed out the
risks of restoring whole files, which HUDLink's installer avoids.

The test car also runs other tweaks from mzd-v74-tweaks, plus its own theme
changes. Those aren't part of HUDLink.

## License

HUDLink is licensed under the GNU Affero General Public License v3.0 or later
([LICENSE](LICENSE)). The bridge comes from mazda-carplay-hud and the HUD
sender from headunit, both licensed under AGPL-3.0, so HUDLink has to use the
same license and can't be released under a permissive license such as MIT.

Anyone can use, modify and share HUDLink. Anyone who shares it, modified or
not, has to provide the source code under the same license. Attribution
details are in [NOTICE.md](NOTICE.md) and [AUTHORS](AUTHORS).

## Disclaimer

HUDLink is an unofficial modification. It isn't affiliated with, endorsed by or
supported by Mazda Motor Corporation, Apple Inc. or Google LLC. Mazda, Mazda
Connect, CarPlay, Apple Maps and Google Maps are trademarks of their respective
owners.

Modifying your head unit's software is at your own risk and may affect your
warranty. The installer checks your head unit and refuses anything it doesn't
recognize, but HUDLink comes with no warranty of any kind (see the license).
