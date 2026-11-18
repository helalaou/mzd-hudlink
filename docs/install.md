# Installing HUDLink

You install HUDLink from a USB drive. Once the drive is ready, the install
takes about five minutes in the car, with the engine running or the ignition in
accessory mode.

## Before you start

- **Check your firmware.** HUDLink needs Mazda Connect firmware 74.00.324. You
  can find it under Settings > System > About > Version Information, where it
  shows `74.00.324` followed by a letter and a region, for example
  `74.00.324A NA`. The installer refuses any other firmware.
- **Remove the original mazda-carplay-hud first**, if you have it, using its
  own uninstaller. HUDLink only touches a `jciCARPLAY` entry that is exactly the
  factory version or exactly its own, and refuses anything else.
- **Check your wireless adapter.** Adapters and hubs with smartBox firmware pass
  route guidance through; the Carabc "smartbox bfcf" hub in our car is one of
  them. A Carlinkit CPC200-U2W doesn't
  ([mazda-carplay-hud#2](https://github.com/KidMixer/mazda-carplay-hud/issues/2)).
  Don't rely on the Wi-Fi password to tell them apart: that issue links
  `12345678` with Carlinkit, but our smartBox hub uses the same password.
- **Get a USB drive** formatted as FAT32, 32 GB or smaller.

## 1. Build the USB files

On a computer with Docker (see [How to run it](../README.md#how-to-run-it) in
the README), run this at the root of the repository:

```bash
tools/make-usb.sh --units imperial
```

For meters and kilometers, use `--units metric` instead. The script builds the
bridge, checks the USB scripts and puts these files in `dist/usb/`:

```
launcher.sh, launcher.sh.md5          the menu
hudlink.sh, hudlink.sh.md5            the installer
hudlink-logs.sh, hudlink-logs.sh.md5  the Save logs step
pkg/hudlink/
  libpatch-blmjcicarplay.so           the bridge
  build-info.txt                      version, units, release or diagnostic build
  manifest.txt, manifest.txt.md5      the md5 of each file above
```

In the car, every file is checked against its md5 first, so a file that got
corrupted on the drive is caught before anything changes.

## 2. Set up the USB drive

The head unit only runs a script from a USB drive if the drive contains a
special trigger file. HUDLink uses the one from piotrmocko's
[mzd-v74-tweaks](https://github.com/piotrmocko/mzd-v74-tweaks) project, which
isn't part of this repository:

1. Download `mzd-v74-tweaks-1.0.1.zip` from its
   [releases page](https://github.com/piotrmocko/mzd-v74-tweaks/releases) and
   unzip it to the root of the drive, following its README. Its `run.sh` starts
   the drive's `launcher.sh`.
2. Copy everything from `dist/usb/` to the root of the drive, replacing
   `launcher.sh` and `launcher.sh.md5`.
3. On a Mac, delete the hidden `._` files macOS leaves on FAT32 drives (replace
   `MYDRIVE` with your drive's name):

   ```bash
   find /Volumes/MYDRIVE -name '._*' -delete
   ```

   A leftover `._` copy of the trigger file can make the head unit start the
   drive twice.

The drive now opens HUDLink's menu instead of the tweaks menu. The tweaks
project's other scripts stay on the drive but aren't used. To get the tweaks
menu back, unzip its release onto the drive again.

## 3. Install in the car

1. Start the car and wait for the head unit to finish booting. CarPlay should
   be connected or ready to connect, because the installer checks the running
   CarPlay service.
2. Plug in the drive. The menu appears after a few seconds.
3. At **STEP 1 of 2: HUDLink**, press **Open**.
4. At **What would you like to do?**, press **Install**. The installer checks
   the files, tests the bridge on your head unit (you'll see "Testing the
   bridge on this head unit...") and then installs it.
5. When you see **HUDLink is installed**, press **OK**.
6. At **STEP 2 of 2: Save logs**, press **Skip**.
7. At **Done. The head unit needs a reboot**, press **Reboot now**. Pull out the
   drive while the screen is black.

After the reboot, start a route in Apple Maps or Google Maps. The arrow and the
distance appear on the HUD as soon as guidance starts, and the HUD clears when
guidance ends.

## Uninstall

Use the same drive, follow the same steps, and press **Uninstall** at step 4.
Every file Install changed goes back to exactly what it was, and the bridge's
folder is deleted. If `NaviSupported` was already on before you installed
HUDLink, it stays on.

## Update

Build fresh USB files, copy them to the drive as in step 2, and press
**Install**. The installer tests the new bridge and swaps it in (you'll see
"HUDLink is updated"). If the new bridge fails a test, your current one stays
in place.

## What the messages mean

Whenever a message ends with **NOTHING was changed**, it means exactly that:
your head unit is just as it was.

| Message | What it means and what to do |
|---|---|
| ... on the USB drive does not match its md5 file, or ... does not match the file list | A file on the drive is damaged or missing. Copy the files from `dist/usb/` again. |
| HUDLink needs CMU firmware 74.00.324 | Your head unit runs different firmware, which HUDLink doesn't support. |
| The CarPlay entry in /jci/sm/sm.conf or sm_WCP.conf is not the factory one or the one HUDLink writes | Another mod has changed the CarPlay service entry, usually the original mazda-carplay-hud. Uninstall that mod first. |
| The device manager settings on the head unit are not in the expected form | `NaviSupported` is either missing or appears more than once. A copy of the file is saved to the drive's `logs/` folder. |
| The CarPlay service is not running with the library path HUDLink expects | Either CarPlay wasn't running, or another mod changed its settings. Start the car normally and try again. |
| Not enough free space on the head unit | The partition named in the message is full, often because of files from other mods. Open an issue and attach `logs/hudlink.log`. |
| The bridge does not fit this head unit, With the bridge, CarPlay would not load the same libraries as today, or The bridge failed a load test | The bridge failed one of its tests on your head unit. Run Save logs and open an issue. |
| HUDLink is already installed | Nothing to do. The second line tells you whether the bridge is already running inside CarPlay. |
| The bridge had switched itself off after a crash | See [The off switch](#the-off-switch) below. The bridge turns back on after the next reboot. |
| A write failed. The factory files were put back | Writing to the head unit failed, so HUDLink isn't installed. Restart the car and try again. |
| A write failed. Every file is still complete (old or new) | Your head unit is safe to use. Run Install or Uninstall again. |

The installer logs everything it does to `logs/hudlink.log` on the drive.

## No directions on the HUD?

Work through these in order:

1. If your car has the navigation SD card, check that the built-in navigation's
   directions appear on the HUD. That confirms the HUD itself is on.
2. Use Apple Maps or Google Maps. Waze doesn't send route guidance to the car,
   and if it's open in the background, close it
   ([mazda-carplay-hud#10](https://github.com/KidMixer/mazda-carplay-hud/issues/10)).
3. Close the navigation app and reopen it from the head unit.
4. In your iPhone's CarPlay settings, forget the car and pair it again.
5. Still nothing? Build the diagnostic version, which keeps a detailed log on
   the head unit:

   ```bash
   tools/make-usb.sh --units imperial --diag
   ```

   Install it and drive a short route with guidance on. Then plug the drive back
   in, press **Skip** at STEP 1, **Open** at **STEP 2 of 2: Save logs**, and
   **Save**. This copies the bridge's logs, the CarPlay service entry and the
   device manager settings to `logs/check-<date>-<time>/` on the drive, without
   changing anything on the head unit. Attach that folder to an issue, then
   reinstall the normal build.

## The off switch

If CarPlay ever crashes while the bridge is running, the bridge writes the crash
details to `/data_persist/cp-hud-mod/off-after-crash.txt` and stays off from
then on, as if it weren't installed. CarPlay keeps working, just without
directions on the HUD.

Running **Install** again copies the crash details to `logs/hudlink-crash.txt`
on the drive and turns the bridge back on from the next reboot. Please attach
that file to an issue so the crash can be looked into.
