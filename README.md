# Mazda HUDLink

**View Your CarPlay directions, on your Mazda's head-up display.** HUDLink bridges
Apple CarPlay, wired or wireless, to the factory head-up display of
first-generation Mazda Connect cars. Apple Maps and Google Maps directions show
up on the HUD just like the built-in SD-card navigation's, and the HUD's speed
and traffic sign display keeps working.

## Why it exists

### CarPlay in Mazdas is a patchwork

Mazda brought Apple CarPlay to its lineup gradually. According to Mazda dealer
information, these are the models and model years that came with CarPlay from
the factory (years can differ by market):

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

A Mazda outside this list that has CarPlay almost certainly had it added after
it left the factory. In late 2018, Mazda started offering an official upgrade
for 2014-and-newer cars with Mazda Connect: dealers install the latest Mazda
Connect firmware and new USB ports, for $199 plus labor
([Car and Driver](https://www.caranddriver.com/news/a25250932/mazda-apple-carplay-android-auto-upgrade/)).
In practice, the install typically comes to around $600 out the door, and that
buys CarPlay on the screen only, with nothing on the HUD. Other cars got
CarPlay from a third-party installer, or from the owner with an aftermarket
adapter. The result is a mix of setups: the official upgrade runs CarPlay over
a USB cable, while wireless CarPlay on these older cars comes from aftermarket
adapters and hubs.

### The head-up display was left out

None of these routes connect CarPlay to the head-up display (Mazda calls it the
Active Driving Display). Mazda only started rolling out CarPlay directions on
the HUD around the 2023–2024 model years, depending on the model and region.
Earlier Mazda Connect cars, roughly the 2014 to 2022–2023 models, only show the
built-in navigation's directions on the HUD. And even on the newer cars, owners
report that HUD directions work most reliably with Apple Maps, while support
for Google Maps and Waze has varied with the model year, the car's software and
iOS and app updates (see the threads on the
[CX-90 Forum](https://www.cx90forum.com/threads/google-maps-on-hud.2299/),
[r/CX5](https://www.reddit.com/r/CX5/comments/1lbisac/turn_by_turn_navigation_in_hud/)
and [Mazdas247](https://mazdas247.com/forum/threads/hud-directions-on-a-pre-2024-cx-5.123881937/)).

So on cars with the first-generation Mazda Connect head unit (CMU150), the HUD
only ever shows directions from the built-in SD-card navigation. It makes no
difference whether CarPlay came from the factory or was added later, or how
genuine the parts are: CarPlay runs on the center screen, its directions never
reach the HUD, and no firmware update for these cars changes that. Even the
official, paid route doesn't help: around $600 at a dealer gets you CarPlay on
the screen, but not on the HUD the way the newest Mazdas have it, and there's
no official update or part a dealer could add to change that. Owners end up
with a HUD that has nothing to do with the navigation they actually use.

### HUDLink connects the two

HUDLink fixes that. On a CMU150 head unit with firmware 74.00.324, it puts live
turn-by-turn directions from Apple Maps and Google Maps on the HUD, wired or
wireless, however CarPlay got into the car. The only requirement is that
CarPlay runs through the head unit itself, as it does with factory CarPlay,
Mazda's official upgrade, or a USB adapter with smartBox firmware. CarPlay itself
keeps running on the center screen as usual.

There's nothing to install on your phone, and no internet connection is needed.
HUDLink simply reads the route guidance your iPhone already sends to the car.

## Works just like the built-in navigation

HUDLink drives the HUD through the same head unit calls the built-in SD-card
navigation uses, so CarPlay directions look and behave like the factory ones:

- **Real-time turn-by-turn.** The distance to the next turn counts down live as
  you drive, and the HUD follows along whenever the route is recalculated.
- **The full set of turn icons:** left and right turns, sharp and slight turns,
  forks and keep left/right, highway changes, on and off ramps, U-turns,
  roundabouts with the exit you need to take, ferries, and your destination.
- **Road names** for the road you're turning onto.
- **Speed and traffic signs still work.** The HUD's own speed readout and
  traffic sign display keep running next to the directions, exactly as they do
  with the built-in navigation.
- **Wired or wireless CarPlay.** The bridge runs inside the head unit's own
  CarPlay process, so it doesn't matter how your iPhone is connected: with a
  cable, or through a wireless adapter with smartBox firmware.
- **Clean handover.** The directions clear when the route ends or CarPlay
  disconnects, and the built-in SD-card navigation keeps working as before.

## What we did

HUDLink is a fork of
[mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud) (AGPL-3.0).
Here's what we changed to get it working in our car and make it something we
trust on the road:

- **We got it working in our car over wireless CarPlay.** We built a diagnostic
  version that logs what happens inside the head unit, found out why the HUD
  stayed blank, and kept testing until Apple Maps and Google Maps directions
  showed up properly on the road.
- **We rewrote the installer from scratch.** The new one tests the bridge on
  your head unit before it changes anything, only touches what it needs to,
  and puts every file back byte for byte when you uninstall.
- **We cleaned up and improved the bridge.** Every source file was reworked.
  Dead code is gone, the structure is clearer, the comments match the code, and
  the bridge is about 350 lines shorter than before. We also added feet and
  miles, an off switch that disables the bridge after a crash, and leaner
  logging.
- **We added proper tooling:** reproducible builds in a pinned container, a
  script that packages the USB drive, and tests that run the installer on a
  fake head unit.

The full list is under [What changed from the original](#what-changed-from-the-original).

## Tested

| | |
|---|---|
| Head unit | Mazda Connect CMU150, firmware 74.00.324A, North America |
| CarPlay | Wireless, through a third-party hub with smartBox firmware (Carabc "smartbox bfcf") |
| Apps | Apple Maps and Google Maps |
| On the HUD | Turn arrows, distance to the turn and road names, alongside the HUD's own speed and traffic sign display, all exactly as they should be |

So far that's one car, driven with wireless CarPlay. Wired CarPlay reaches the
bridge through exactly the same path inside the head unit. Other cars on
firmware 74.00.324 should behave the same way, and the installer checks each
head unit before it touches anything.

## How to run it

### What you need

- A Mazda with the first-generation Mazda Connect head unit (CMU150) on
  firmware 74.00.324, and a head-up display. You can check the firmware under
  Settings > System > About > Version Information.
- An iPhone with CarPlay, connected by cable or through a wireless adapter with
  smartBox firmware (see [Known issues](#known-issues)).
- A computer with git and Docker: Docker Engine on Linux, or
  [Colima](https://github.com/abiosoft/colima) or Docker Desktop on macOS.
- A USB drive formatted as FAT32, 32 GB or smaller.
- If you installed the original mazda-carplay-hud, uninstall it first with its
  own uninstaller.

### 1. Build the USB files

Start Docker (with Colima, run `colima start`), then run this in a terminal at
the root of this repository:

```bash
tools/make-usb.sh --units imperial
```

Use `--units metric` instead for meters and kilometers. The first run sets up
the build container and downloads the ARM cross-compiler, so it takes a while;
later runs are much faster. Everything ends up in `dist/usb/`.

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

### 3. Install in the car

1. Start the car (engine on or accessory mode) and wait for the head unit to
   finish starting up.
2. Plug in the drive. After a few seconds, the HUDLink menu appears.
3. At **STEP 1 of 2: HUDLink**, press **Open**, then **Install**. The installer
   tests the bridge on your head unit before it changes anything.
4. When it says **HUDLink is installed**, press **OK**, then press **Skip** at
   STEP 2.
5. Press **Reboot now**, and pull out the drive while the screen is black.

### 4. Drive

Start a route in Apple Maps or Google Maps. The arrow, the distance and the road
name appear on the HUD as soon as guidance starts, and the HUD clears when it
ends.

To uninstall, run the drive again and choose **Uninstall**. The
[install guide](docs/install.md) covers updates, every message the installer
can show, and what to try if the HUD stays blank.

Here's everything Install changes. Uninstall puts all of it back:

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
device manager's settings. That setting is what tells the iPhone the car can
accept route guidance. For the full picture, including the message formats,
see [docs/how-it-works.md](docs/how-it-works.md).

## Building from source

`tools/make-usb.sh` builds the bridge for you, so you only need this section if
you're working on the code.

```bash
tools/build.sh UNITS=imperial
```

The bridge lands in `mazda/build/release/libpatch-blmjcicarplay.so`. Builds are
reproducible: in the pinned build container, the same source and options always
produce the same bytes.

| Option | What it does |
|---|---|
| `UNITS=metric` | meters and kilometers (the default) |
| `UNITS=imperial` | feet and miles |
| `VIETNAMESE=1` | replaces Vietnamese letters the HUD's font can't display |
| `diag` | builds the diagnostic version, which keeps a detailed log on the head unit |
| `debug` | builds an unoptimized version that logs everything |

You can combine them, for example `tools/build.sh diag UNITS=imperial`. To put
the diagnostic version on a USB drive, run
`tools/make-usb.sh --units imperial --diag`.

If you'd rather not use Docker, you can run `make` directly on Linux x86-64
with a checkout of [lmagder/m3-toolchain](https://github.com/lmagder/m3-toolchain)
at commit `61ec0343`:

```bash
make -C mazda M3TOOLCHAIN=/path/to/m3-toolchain UNITS=imperial
```

### Tests

```bash
tools/test.sh
```

This runs the USB scripts against a fake head unit using BusyBox, the same
shell and tools the real head unit has. The tests cover every refusal, install,
update, uninstall back to the original bytes, writes that fail halfway, the off
switch, Save logs and the launcher.

To test against your own car's files as well, add `--factory DIR` with copies
of your head unit's `sm.conf` and `sm_WCP.conf` (details in
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
  hudlink-logs.sh            the Save logs step
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
  stays blank. Worse, if Waze is left open in the background, it can freeze the
  HUD on the last turn after Apple Maps or Google Maps has finished guiding.
  Closing Waze clears it
  ([mazda-carplay-hud#10](https://github.com/KidMixer/mazda-carplay-hud/issues/10)).
- **Some wireless adapters block route guidance.** A Carlinkit CPC200-U2W
  leaves the HUD blank, while adapters with smartBox firmware, like the one in
  our car, work fine
  ([mazda-carplay-hud#2](https://github.com/KidMixer/mazda-carplay-hud/issues/2)).
- **CarPlay guidance and built-in navigation guidance at the same time** hasn't
  been tested.
- **Only firmware 74.00.324 is supported.** The installer refuses anything else.

### The off switch

If CarPlay ever crashes while the bridge is running, the bridge saves the crash
details on the head unit and stays off until you run Install again, so a fault
can't repeat on every drive. CarPlay keeps working; you only lose the
directions on the HUD. Running Install again turns the bridge back on and
copies the crash details to the USB drive so you can report them.

This happened once during testing, with an earlier build: CarPlay aborted,
possibly while it was shutting down. The off switch did its job and CarPlay
kept running. We never found the cause, and the build tested after that ran
without problems.

## What changed from the original

Compared with [mazda-carplay-hud](https://github.com/KidMixer/mazda-carplay-hud)
at commit `9782e92`:

- **A new installer.** It only edits the `jciCARPLAY` entry on the head unit and
  refuses any entry it doesn't recognize. That means it ships no Mazda files
  and doesn't disturb changes made by other mods. Before anything points to the
  bridge, it tests it on the head unit: its libraries and symbol versions,
  CarPlay's library list with and without it, and a load test. Every file is
  written through a checked temporary copy, and if a write fails, the factory
  files go back. Updates replace the installed bridge in place, and Uninstall
  removes exactly what Install added, nothing more.
- **CarPlay keeps its factory library path.** The original installer gives the
  CarPlay service a shorter `LD_LIBRARY_PATH`. HUDLink leaves the factory path
  alone, so CarPlay loads exactly the same libraries it would without the
  bridge, and the installer confirms this before changing anything.
- **Feet and miles** (`UNITS=imperial`).
- **An off switch** that disables the bridge after a crash (see
  [The off switch](#the-off-switch)).
- **A diagnostic build** (`make diag`) and a **Save logs** step that copies
  everything a bug report needs onto the USB drive.
- **Leaner logging.** All log lines go to one size-capped file, and the
  guidance request retries are only logged when the result changes.
- **Reproducible builds** in a pinned toolchain container, with a fixed link
  order and automatic rebuilds when the build options change.
- **Tests** for the USB scripts, run on a fake head unit.
- **Cleaner code.** The compiled-out speed limit code, an unused copy of
  headunit's D-Bus proxies and the leftover reverse-engineering switches are
  gone. The HUD sender now builds each frame in a single path, and the comments
  describe the code as it actually is.

## Credits

HUDLink uses work from these projects:

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
  cross-compiler we build the bridge with. The build container downloads it;
  it isn't part of this repository.
- **piotrmocko**, [mzd-v74-tweaks](https://github.com/piotrmocko/mzd-v74-tweaks)
  (CC BY-NC-SA 4.0). Its release sets up the USB drive that starts HUDLink.
  None of its files are included here, and HUDLink's USB scripts were written
  from scratch.

We also relied on findings from the original project's issues: ibm0078 and
AmbrosiaAS worked out which wireless adapters pass route guidance through
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
([LICENSE](LICENSE)). That wasn't our choice to make: the bridge comes from
mazda-carplay-hud and the HUD sender from headunit, both AGPL-3.0, and anything
built on them has to stay AGPL-3.0. So HUDLink can't be released under a
permissive license like MIT.

In practice, anyone can use, modify and share HUDLink. If you share it,
modified or not, you have to share the source code under the same license. Full
attribution is in [NOTICE.md](NOTICE.md) and [AUTHORS](AUTHORS).

## Disclaimer

HUDLink is an unofficial modification. It isn't affiliated with, endorsed by or
supported by Mazda Motor Corporation, Apple Inc. or Google LLC. Mazda, Mazda
Connect, CarPlay, Apple Maps and Google Maps are trademarks of their respective
owners.

You modify your head unit's software at your own risk, and doing so may affect
your warranty. The installer checks your head unit and refuses anything it
doesn't recognize, but HUDLink comes with no warranty of any kind (see the
license). And please keep your eyes on the road.
