# Background

This page explains why the head-up display in most Mazdas with CarPlay never
shows CarPlay directions, and where HUDLink fits in.

## CarPlay in Mazdas

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

## CarPlay and the head-up display

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

## What HUDLink does

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
[How HUDLink works](how-it-works.md) describes the data path in detail.
