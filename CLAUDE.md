# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A fork of [K3NG's Arduino rotator controller](https://github.com/k3ng/k3ng_rotator_controller), configured specifically
for a **RemoteQTH Rotator Interface v3.3** board (Arduino Nano, ATmega328P, old bootloader) driving an **azimuth-only**
rotator. It's a single Arduino firmware project, not a library — there is no separate app/service layered on top.

- `origin` → `sq9fk/k3ng_controler_nano_light` (this fork)
- `upstream` → `k3ng/k3ng_rotator_controller` (the original project; pull from here to get upstream fixes)

## Build / flash / monitor

PlatformIO is the primary toolchain for this fork (`platformio.ini` at repo root, targeting the `nanoatmega328` board —
old bootloader, 30720 B flash / 2048 B RAM, matching the physical board):

```
pio run                 # compile, prints flash/RAM usage
pio run -t upload       # compile + flash over USB
pio device monitor      # serial monitor at 9600 baud (CONTROL_PORT_BAUD_RATE in rotator_settings.h)
pio run -t clean        # clean build artifacts
```

The sketch also still opens directly in the Arduino IDE (`k3ng_rotator_controller/k3ng_rotator_controller.ino`) if
needed — it's kept as a `.ino` for that reason. For Arduino IDE use, the vendored `libraries/` subfolders need to be on
the IDE's library path (e.g. symlink/copy into your sketchbook's `libraries/` folder); PlatformIO does **not** need this
(see the library note below).

There is no test suite — this is firmware. "Verification" means: it compiles, flash/RAM usage is within budget (see
below), and ultimately behavior is checked on the physical board (GS-232 commands over serial, motor relays, position
readback).

### Library dependency gotcha (PlatformIO-specific)

`libraries/` contains ~15 vendored, Arduino-IDE-style third-party libraries used by *optional* K3NG features (GPS, RTC,
compass ICs, stepper motor via `TimerFive`/`TimerOne`, etc.). Do **not** add that whole folder via `lib_extra_dirs`.
PlatformIO's dependency finder does a textual `#include` scan that ignores `#ifdef` guards, so pointing it at the whole
tree pulls in every library regardless of which features are actually enabled — including Mega-only libraries
(`TimerFive`) that don't even compile for the ATmega328P. If you enable a feature that needs one of those vendored
libraries, add just that one subfolder to `lib_extra_dirs`, not the whole `libraries/` tree.

## Architecture

Everything lives in `k3ng_rotator_controller/`: one large `.ino` (the state machine, command parsing, `setup()`/`loop()`)
plus `rotator_*.h`/`.cpp` files that are `#include`d by it (not a library — direct textual includes, same translation
unit). There's no OOP layering to trace through; the mental model is a single flat polling loop plus a large set of
compile-time feature switches.

**Feature-flag driven compilation.** Nearly everything optional (position sensor type, display type, GPS, RTC, moon/sun
tracking, master/slave linking, protocol emulation) is a `#define FEATURE_...` / `OPTION_...` in `rotator_features.h`,
consumed via `#ifdef` throughout the `.ino`. `rotator_dependencies.h` is the compile-time rule-checker for this system —
it `#error`s on incompatible combinations (e.g. two control protocols at once) and auto-derives some flags. Read it
before adding/removing a `FEATURE_*` to understand what else it implies or conflicts with.

**Three config files, always edited together:**
- `rotator_features.h` — what's compiled in
- `rotator_pins.h` — which Arduino pin does what (only meaningful for the features enabled above)
- `rotator_settings.h` — tunable values (timeouts, calibration tables, LCD geometry, rotation limits)

The repo also ships alternate `rotator_features_<board>.h` / `rotator_pins_<board>.h` / `rotator_settings_<board>.h`
triads (`_m0upu`, `_wb6kcn`, `_wb6kcn_k3ng`, `_test`) from upstream, selected via `HARDWARE_*` defines in
`rotator_hardware.h`. **This fork does not use that mechanism** — since the whole repo targets one board, the *default*
`rotator_features.h` / `rotator_pins.h` / `rotator_settings.h` were edited directly instead of adding a new
`HARDWARE_REMOTEQTH_V33` variant. Keep doing it that way; don't introduce a parallel hardware profile for this single-board
fork.

**Control flow:** `setup()` initializes serial, peripherals, EEPROM-stored settings, pins, display, encoders, interrupts.
`loop()` is a flat sequence of `check_*()` / `service_*()` / `read_*()` calls gated by `#ifdef` (serial command parsing,
position reads, rotation state machine, button/preset checks, display refresh) — no RTOS, no async scheduler. Interrupts
are only used for rotary/incremental encoder and pulse-position inputs.

## RemoteQTH v3.3 hardware mapping (verified, not guessed)

Pin assignments were originally derived by tracing the actual KiCad schematic/PCB netlist for this board
(`RotatorInterface.sch`/`.kicad_pcb`, rev 3.3, by OK1HRA, from `remoteqth.com/hw/rotator_interface_33.zip`) — following
copper connections through the relay-driver transistors and filter networks, not inferred from silkscreen labels alone.
Some pins were later moved from that baseline due to real physical rewiring done on the board (noted below) — when in
doubt, physical wiring as reported by whoever has the board in hand wins over the schematic trace.

| Signal | Pin | Notes |
|---|---|---|
| CW relay | D6 | via R3→Q3→RL1 |
| CCW relay | D7 | via R2→Q2→RL2 |
| Brake relay | D8 | via R1→Q1→RL3 |
| Azimuth position | pulse input, D2 (INT0) | rotor position sensor is a reed switch/"contactron" (dry contact), not analog voltage — `FEATURE_AZ_POSITION_PULSE_INPUT`; calibrate `AZ_POSITION_PULSE_DEG_PER_PULSE` in `rotator_settings.h` against the actual rotor spec; `OPTION_POSITION_PULSE_INPUT_PULLUPS` must stay enabled since a dry contact needs the internal pull-up or D2 floats between pulses |
| AREF | external | board supplies its own reference through R9, independent of which position-sensor feature is active — keep `OPTION_EXTERNAL_ANALOG_REFERENCE` enabled regardless, since switching to the internal reference while an external one is wired to AREF risks damaging the chip |
| Manual CW/CCW jog buttons | CW=A5, CCW=A4 | moved off A2/A3 to free A2 for LCD D7 |
| LCD (16x2, 4-bit) | RS=D12, E=D11, D4=D5, D5=D4, D6=D3, D7=A2 | D7 moved off D2 to free D2 for the azimuth pulse input above |
| AZ preset rotary encoder | D10 / D9 | |

The board is azimuth-only (`FEATURE_ELEVATION_CONTROL` stays off) and assumes a 450°-capable rotator
(`AZIMUTH_ROTATION_CAPABILITY_EEPROM_INITIALIZE`). Two things that must stay consistent with that 450° figure, because
this codebase has no single "rotation capability" switch — every subsystem that handles the azimuth range does its own
thing, and several default to a hardcoded 360°:
- `OPTION_AZ_POSITION_PULSE_HARD_LIMIT` must stay **enabled** — without it, the pulse-counted azimuth wraps at a
  hardcoded 360° instead of clamping at `azimuth_starting_point + azimuth_rotation_capability` (450°), corrupting
  position tracking once the rotor swings into the overlap zone.
- `OPTION_PRESET_ENCODER_0_360_DEGREES` must stay **disabled** — enabled, it clamps the front-panel preset knob at a
  hardcoded 360°, making the 360°-450° overlap zone unreachable via the encoder.

### Fork-added code: AZ pulse debounce (not upstream)

Upstream K3NG only has pulse debounce for elevation (`OPTION_EL_PULSE_DEBOUNCE`/`EL_POSITION_PULSE_DEBOUNCE`) — there
was no azimuth equivalent. Since this board's azimuth sensor is a mechanical reed switch (bounce-prone), the same
pattern was ported to azimuth: `OPTION_AZ_PULSE_DEBOUNCE` (`rotator_features.h`) gates a debounce window,
`AZ_POSITION_PULSE_DEBOUNCE` (`rotator_settings.h`, default 20 ms) sets it, and `az_position_pulse_interrupt_handler()`
in the `.ino` has the same `#ifdef .../#else/#endif` structure as its EL counterpart. **Do not copy the EL default of
500 ms here** — at `AZ_POSITION_PULSE_DEG_PER_PULSE` = 0.5°, a fast-moving rotator can generate real pulses much faster
than every 500ms, and a debounce window longer than the real inter-pulse gap silently drops genuine pulses (worse than
the bounce it's meant to fix). Keep this value well below the shortest real gap between pulses at full rotation speed.
Since this whole option doesn't exist upstream, `git fetch upstream` merges won't touch it, but also won't ever
reintroduce or update it — it's this fork's to maintain.

## Flash/RAM budget (read before enabling any new FEATURE_*)

The ATmega328P old-bootloader Nano has only **30720 B flash / 2048 B RAM**, and this codebase is large. With
`FEATURE_4_BIT_LCD_DISPLAY` + `FEATURE_AZ_PRESET_ENCODER` both on, a "stock" build overflows flash by ~1.3 KB. Two
things bought back headroom (currently ~52% flash / ~50% RAM used):

1. `OPTION_SAVE_MEMORY_EXCLUDE_EXTENDED_COMMANDS` and `OPTION_SAVE_MEMORY_EXCLUDE_BACKSLASH_CMDS` in
   `rotator_features.h` — K3NG's own built-in size-reduction switches. They strip a large block of rarely-used
   extended/`\`-prefixed serial commands; core GS-232 rotate/query commands are unaffected.
2. Disabling `OPTION_*` flags tied to features this board doesn't use at all (GPS, RTC, clock, moon/sun/satellite
   tracking, elevation, stepper motor) — these were previously enabled as dead weight in the default config.

**After enabling any new feature, run `pio run` and check the flash/RAM percentages in the output before considering the
change done.** There is very little margin left before another feature addition will overflow again.
