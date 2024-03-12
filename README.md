# SNES Controllers to USB

This project contains the hardware design files and firmware used to build a 4
SNES controller hub to USB. At its core the board uses a Raspberry Pi Pico W
(could technically also be a regular Raspberry Pi Pico, but I have not tried
this) to poll the SNES controllers and manage USB communications with the host.

The design files are made using KiCAD, and the firmware is written in C++ using
the pico-sdk and FreeRTOS.

## Layout

The toplevel files are the hardware design files.

The software is in the `firmware/` directory. This has its own README.md.

The `graphics/` directory contains schematics and PCB diagrams in
easier-to-access formats for human use.

## Licenses

See the LICENSE files in the top level directory and in the `firmware/` folder
for more details.

In summary, the hardware files are released under the "CERN Open Hardware
Licence Version 2 - Weakly Reciprocal" license, and the software is
dual-licensed under the LGPL-2.1 or later and GPL-2.0 or later.
