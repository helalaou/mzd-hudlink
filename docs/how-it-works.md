# How HUDLink works

This page traces a CarPlay instruction from the iPhone all the way to the
head-up display, then explains how the installer keeps the head unit safe. The
source files mentioned here contain the same details in their comments, right
next to the code.

## Why CarPlay directions never reach the HUD

On Mazda Connect CMU150 firmware 74.00.324, the HUD displays whatever the head
unit's navigation service, `com.jci.vbs.navi`, sends it, and only the built-in
navigation ever sends it anything. The head unit's CarPlay process,
`jciCARPLAY`, shows CarPlay on the center screen and keeps track of who is
giving directions. But it never asks the iPhone for route guidance, and it has
no code to turn CarPlay's maneuvers into HUD frames. HUDLink adds both, running
inside that same process.

## The data path

```
iPhone, navigating in Apple Maps or Google Maps
  |  iAP2 route guidance updates, over a cable or a wireless adapter
  v
device manager (ipoddev) -> System V message queue
  |
  v
jciCARPLAY  <- libpatch-blmjcicarplay.so, preloaded with LD_PRELOAD
  |  devmgr_shim.cpp   msgrcv() hook: sees every message the process receives
  |  nav.cpp           decodes maneuvers and guidance, picks the one to show
  |  hud/hud_send.cpp  builds the HUD frame and sends it from its own thread
  v
com.jci.vbs.navi, through the head unit's libjcidbus and libjcivbsnaviclient
  |  SetHUDDisplayMsgReq   turn icon and distance
  |  SetHUD_Display_Msg2   road name
  v
Active Driving Display
```

Two settings make this work, and the installer writes both:

- `LD_PRELOAD` in the `jciCARPLAY` entry of `/jci/sm/sm.conf` and
  `/jci/sm/sm_WCP.conf` loads the bridge into the CarPlay process. The head
  unit uses one file or the other depending on its board type, so both get the
  entry.
- `NaviSupported` set to `TRUE` in `/etc/devmgr_config_master.xml` makes the
  device manager tell the iPhone that the car accepts route guidance.

## Staying out of the way: main.cpp

Because of `LD_PRELOAD`, the bridge's constructor runs in every process that
inherits the variable. It checks `/proc/self/cmdline` and only becomes active
in the CarPlay process, which the system manager starts as
`sm_svclauncher -l jciCARPLAY /jci/carplay/blmjcicarplay.so 0 -a`. In every
other process, the hooks simply pass each call through untouched.

The constructor also installs the fatal-signal handlers behind the off switch
(see below).

## Asking for route guidance: devmgr_shim.cpp

`jciCARPLAY` talks to the iPhone through the device manager's client library,
`libdevmgr_interface.so`. Once a CarPlay session is up, it calls
`eDevMgriPodStartCallStateUpdates`, `eDevMgriPodStartCommunicationUpdates` and
`eDevMgriPodSendLocationInformation`. The bridge wraps all three: it passes
each call on unchanged, then arms navigation. Arming does three things:

1. It starts the HUD sender.
2. It subscribes to two CarPlay status signals through the head unit's own
   `libjcicarplay_client.so`: `TurnByTurnEntitySignal`, which says who is
   giving directions (0 nobody, 1 CarPlay, 2 the built-in navigation), and
   `SessionDeactiveSignal`, which fires when the session ends. The HUD is
   cleared when CarPlay stops giving directions and when the session ends.
3. It calls `eDevMgriPodStartNaviRouteGuidenceUpdates` to ask the iPhone for
   route guidance, a call `jciCARPLAY` never makes on its own. A background
   thread repeats the request every 5 seconds for as long as the process runs.

## Reading route guidance: the msgrcv() hook

The device manager's receive thread runs inside `jciCARPLAY` and pulls each
iAP2 event off a System V message queue with `msgrcv()`. The bridge's version
of `msgrcv()` calls the real function first. Once navigation is armed, it
passes the two message types it cares about on to `nav.cpp`. It never modifies
a message, so CarPlay receives exactly what it would without the bridge.

## Decoding: nav.cpp

The device manager converts the iPhone's updates into two fixed-layout
messages. The offsets below come from disassembling the device manager on
firmware 74.00.324.

**Maneuver, type `0x8059`, 840 bytes:** one message per maneuver on the current
route.

| Offset | Type | Field |
|---|---|---|
| `0x1c` | u32 | validInfo: which optional fields are present (`0x10` road name, `0x800` roundabout junction) |
| `0x20` | u32 | index, in the upper 16 bits |
| `0x24` | char[256] | maneuverDesc, the instruction text |
| `0x124` | u32 | length of maneuverDesc |
| `0x128` | u32 | maneuverType |
| `0x12c` | char[256] | afterManeuverRoadName, the road the maneuver leads onto |
| `0x33c` | u32 | driveSide: 0 right-hand traffic, 1 left-hand traffic |
| `0x340` | u32 | junctionType: 0 intersection, 1 roundabout |
| `0x346` | s16 | junctionElementExitAngle: the bearing of the exit taken; 0 is straight on, negative is left, positive is right |

**Guidance, type `0x8058`, 1628 bytes:** the route status.

| Offset | Type | Field |
|---|---|---|
| `0x34c` | u32 | distance to the next maneuver, in meters |
| `0x458` | u16 | maneuverList[0], the index of the next maneuver |
| `0x656` | u16 | number of entries in maneuverList |

### Picking the maneuver to show

Every time the iPhone recalculates the route, it resends the whole maneuver
list starting from index 0. That means an index is just a position in the
latest list, not a stable ID.

The bridge buffers the maneuvers in the current list but skips the entries that
only describe the road you're already on: types 0 NoTurn, 5 FollowRoad,
11 StartRoute and 18 StartRouteWithUTurn. Index 0 is always one of these.

Each guidance message names the next maneuver in maneuverList[0]. The HUD shows
the buffered maneuver with that index, or the one after it if that index is one
of the skipped entries. If a new list is still arriving and the maneuver isn't
buffered yet, the HUD keeps showing what it already has.

The road name comes from the selected maneuver. Apple leaves it empty for many
turns, and in that case the instruction text is shown instead.

### From maneuver type to HUD icon

`maneuverType` uses the same values as Apple's `CPManeuverType`. The HUD
sender, on the other hand, expects Android Auto's turn events (the values
headunit uses), so `classify()` translates between the two:

| maneuverType | HUD |
|---|---|
| 1 Left, 20 LeftTurnAtEnd; 2 Right, 21 RightTurnAtEnd | turn left; turn right |
| 47 SharpLeft; 48 SharpRight | sharp turn |
| 49 SlightLeft; 50 SlightRight | slight turn |
| 13 KeepLeft; 14 KeepRight; 51-53 ChangeHighway | fork |
| 4 UTurn, 26 UTurnWhenPossible | U-turn, on the side the traffic drives on |
| 8 OffRamp, 22 and 23 HighwayOffRamp; 9 OnRamp | off ramp; on ramp |
| 6, 7, 19, 28-46 RoundaboutExit1..19, or junctionType 1 | roundabout, one of 12 icons chosen from the exit angle |
| 15-17 Ferry | ferry |
| 10, 12, 27 Arrive; 24 ArriveLeft; 25 ArriveRight | destination |
| 3 Straight, the skipped types, anything else | straight on |

### Distance

The guidance message gives the distance in meters. The metric build shows
meters under 1 km and kilometers beyond that. The imperial build
(`UNITS=imperial`) shows feet, rounded to the nearest 10, under 0.1 mile, and
miles to one decimal place beyond that.

## Sending to the HUD: hud/hud_send.cpp

The sender is adapted from [headunit](https://github.com/Trevelopment/headunit)'s
HUD code. Rather than using a D-Bus binding of its own, it goes through the head
unit's own libraries, `libjcidbus` and `libjcivbsnaviclient` (declared in
`oem/`), and makes the same calls the built-in navigation makes. This has two
useful side effects: `libjcidbus` keeps the process alive if the service bus
disappears (for example when switching between CarPlay and Android Auto), and
it runs its own I/O thread.

The decoder runs on the device manager's receive thread, which must never
block. So it just writes a snapshot under a sequence lock and wakes the sender
thread. The sender compares the snapshot with what it last sent and makes the
calls: `SetHUDDisplayMsgReq` for the icon and the distance,
`SetHUD_Display_Msg2` for the road name.

A ticker wakes the sender twice a second to resend the current frame, just as
the built-in navigation does, so the HUD doesn't time it out. The sender also
clears the HUD at startup, because the HUD keeps its last frame across a
restart, and whenever directions end.

The HUD's font can draw ASCII and two-byte UTF-8 characters, but it drops
three-byte ones. With `VIETNAMESE=1`, Vietnamese letters with two diacritics
are replaced with the closest letter the font has. Otherwise, road names are
sent as they are.

## Logs, the diagnostic build and the off switch

- **Release build:** logs errors only, to `/tmp/carplay_bridge.log`, capped at
  256 KB. `/tmp` lives in RAM, so the log starts fresh whenever the head unit
  restarts.
- **Diagnostic build** (`make diag`): logs at debug level to
  `/data_persist/cp-hud-mod/diag.log`. Each start of CarPlay gets a new file,
  and the two before it are kept as `diag.1.log` and `diag.2.log`. Each file is
  capped at 384 KB and begins with the process's command line and library
  settings. Guidance request retries are logged at first, then only when the
  result changes, plus about once every five minutes. The Save logs step copies
  these files to the USB drive.
- **Off switch:** if the CarPlay process dies from a fatal signal (`SIGSEGV`,
  `SIGBUS`, `SIGABRT`, `SIGFPE` or `SIGILL`) while the bridge is active, the
  handler writes the signal, the registers, a backtrace and the memory map to
  `/data_persist/cp-hud-mod/off-after-crash.txt`, using only async-signal-safe
  calls. It then lets the process die exactly as it would have anyway. As long
  as that file exists, the bridge stays inactive on every start, so a fault
  can't repeat on every drive. Running Install again copies the file to the USB
  drive and deletes it, which turns the bridge back on.

## The installer: usb/hudlink.sh

The installer is a POSIX shell script written for the head unit's BusyBox. It
ships no Mazda files. Instead, it edits the files already on the head unit, and
only when they're in a state it recognizes.

**States.** Each `sm` config file is classed as `orig` (the factory
`jciCARPLAY` entry), `new` (exactly the entry HUDLink writes) or `other`. An
entry only counts as `orig` or `new` if applying or undoing the edit gives back
the file byte for byte, and if it has the expected number of `environ_var`
lines. Files with CR line endings or with two `jciCARPLAY` entries count as
`other`, and the installer won't touch them. `NaviSupported` must appear
exactly once, set to `TRUE` or `FALSE`.

**Checks before any change.** The installer verifies:

- its own md5, the md5 of the file list and the md5 of every file it brings;
- the firmware version, 74.00.324;
- that the running CarPlay service has the factory library path;
- the free space on `/data_persist`, `/jci` and `/etc`.

**Testing the bridge on the head unit.** The new bridge is copied next to the
installed one under a temporary name, then tested with the CarPlay service's
library path before anything points to it:

1. The dynamic loader traces its libraries with `LD_BIND_NOW` and `LD_WARN`
   set, which catches a missing library or symbol version.
2. CarPlay's library list is traced with and without the bridge preloaded.
   Apart from the bridge itself, the two lists must match.
3. The bridge is preloaded into `/bin/true`, a throwaway program, which must
   run and print nothing.

If any test fails, nothing on the head unit has changed.

**Writing files.** Each file is written to a temporary copy that keeps the
original's owner and permissions. The copy is checked against the expected md5
and then renamed over the original in a single step, so a file is always
either the old version or the new one, never something in between.
`NaviSupported` is only changed if that one value is the only difference and
changing it back gives the current file. If a write fails during Install, the
factory state is restored in a safe order: the service entries first, then
`NaviSupported`, and finally the bridge's folder, once nothing points to it
anymore.

**Uninstall** runs the same process in reverse. If `NaviSupported` was already
on before HUDLink was installed (the installer records this in
`navi-was-on.txt`), it stays on.

**Exit codes.** `launcher.sh` reads the installer's exit code to decide whether
to offer a reboot: 0 means nothing changed, 1 means it failed and nothing
changed, and 2 means something changed.

`tests/run.sh` exercises every one of these paths on a fake head unit.
