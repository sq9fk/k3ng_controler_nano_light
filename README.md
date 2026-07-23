# K3NG Rotator Controller — RemoteQTH v3.3 / Nano "light" fork

A trimmed-down fork of the [K3NG Arduino rotator controller](https://github.com/k3ng/k3ng_rotator_controller),
configured for exactly one hardware setup: a **RemoteQTH Rotator Interface v3.3** board (Arduino Nano, ATmega328P,
old bootloader) driving an **azimuth-only, 450°-capable** rotator whose position sensor is a **reed switch /
contactron pulse output**.

This is not a general-purpose build. Every optional K3NG subsystem that this board does not use (elevation, GPS,
RTC/clock, ethernet, moon/sun/satellite tracking, stepper motor, compass/accelerometer sensors, alternate display
types) is disabled, and the alternate-hardware profile files shipped upstream have been removed — the ATmega328P
does not have the flash to carry them.

Upstream's general project description, protocol documentation and feature list still apply and live in the
[K3NG wiki](https://github.com/k3ng/k3ng_rotator_controller/wiki).

## Hardware

| Item | Value |
|---|---|
| Board | RemoteQTH Rotator Interface v3.3 (OK1HRA), Arduino Nano ATmega328P, old bootloader |
| Flash / RAM | 30720 B / 2048 B |
| Axes | Azimuth only (`FEATURE_ELEVATION_CONTROL` off) |
| Rotator | 450° rotation capability, starting point 180° |
| Position sensor | Pulse input (dry contact reed switch), 0.5° per pulse |
| Display | 16×2 HD44780 LCD, 4-bit parallel |
| Controls | Manual CW/CCW jog buttons + rotary encoder for azimuth preset |
| Control protocol | Yaesu GS-232B emulation over USB serial, 9600 baud |

### Pin map (traced from the rev 3.3 KiCad schematic, plus later physical rewiring)

| Signal | Pin | Notes |
|---|---|---|
| CW relay | D6 | R3 → Q3 → RL1 |
| CCW relay | D7 | R2 → Q2 → RL2 |
| Brake relay | D8 | R1 → Q1 → RL3, 3 s engage delay, active HIGH = disengaged |
| Azimuth position pulse | D2 (INT0) | interrupt-capable pin required; internal pull-up enabled |
| Manual CW / CCW buttons | A5 / A4 | moved off A2/A3 to free A2 for the LCD |
| AZ preset rotary encoder | D10 (CW) / D9 (CCW) | half-step mode, pull-ups enabled |
| Rotation stall indicator | D13 | onboard Nano LED, no external wiring; goes high when a stall is detected |
| LCD RS / E | D12 / D11 | |
| LCD D4 / D5 / D6 / D7 | D5 / D4 / D3 / **A2** | LCD D7 moved off D2 to free D2 for the position pulse input |
| AREF | external | supplied by the board through R9; no software setting needed, see below |

Unused K3NG outputs (PWM speed control, speed/preset pots, overlap LED, serial LED, stop button) are set to `0`
(disabled) in `rotator_pins.h`.

## Enabled feature set

Everything below is what is actually compiled in (`rotator_features.h`); anything not listed is off.

```
FEATURE_YAESU_EMULATION              GS-232 command set on the control port
OPTION_GS_232B_EMULATION             GS-232B dialect (not A)
FEATURE_AZ_POSITION_PULSE_INPUT      pulse-counted azimuth from the reed switch
FEATURE_4_BIT_LCD_DISPLAY            16x2 HD44780
FEATURE_AZ_PRESET_ENCODER            front-panel preset knob
OPTION_ENCODER_HALF_STEP_MODE
OPTION_ENCODER_ENABLE_PULLUPS
OPTION_POSITION_PULSE_INPUT_PULLUPS  required: a dry contact leaves D2 floating without it
OPTION_AZ_POSITION_PULSE_HARD_LIMIT  required for 450 deg (see below)
OPTION_AZ_PULSE_DEBOUNCE             fork-added, not in upstream (see below)
OPTION_AZ_MANUAL_ROTATE_LIMITS       jog buttons stop at the software end stops
FEATURE_AZ_ROTATION_STALL_DETECTION  cuts the motor if position stops changing mid-rotation
OPTION_ROTATION_STALL_DETECTION_SERIAL_MESSAGE
OPTION_DISPLAY_STATUS                LCD row 1 status
OPTION_DISPLAY_HEADING_AZ_ONLY       LCD row 1 heading
OPTION_SAVE_MEMORY_EXCLUDE_EXTENDED_COMMANDS
OPTION_SAVE_MEMORY_EXCLUDE_BACKSLASH_CMDS
LANGUAGE_ENGLISH
```

### Key settings (`rotator_settings.h`)

| Setting | Value |
|---|---|
| `AZIMUTH_STARTING_POINT_EEPROM_INITIALIZE` | 180 |
| `AZIMUTH_ROTATION_CAPABILITY_EEPROM_INITIALIZE` | 450 |
| `AZ_POSITION_PULSE_DEG_PER_PULSE` | 0.5 |
| `AZ_POSITION_PULSE_DEBOUNCE` | 20 ms |
| `AZIMUTH_TOLERANCE` | 3.0° |
| `AZ_MANUAL_ROTATE_CCW_LIMIT` / `CW_LIMIT` | 182 / 628 (raw azimuth, 2° inside the mechanical stops) |
| `STALL_CHECK_FREQUENCY_MS_AZ` / `_DEGREES_THRESHOLD_AZ` | 4000 ms / 2° (window must outlast the rotor's start-up ramp) |
| `AZ_BRAKE_DELAY` | 3000 ms |
| `LCD_COLUMNS` × `LCD_ROWS` | 16 × 2, 1000 ms refresh |
| `CONTROL_PORT_BAUD_RATE` | 9600 |

These values are EEPROM-initialized on first run only; changing the starting point or rotation capability later
means re-initializing EEPROM or setting it over the serial command interface.

## Build, flash, monitor

PlatformIO is the primary toolchain (`platformio.ini` at the repo root, env `nanoatmega328`):

```bash
pio run
```

```bash
pio run -t upload
```

```bash
pio device monitor
```

```bash
pio run -t clean
```

The sketch is deliberately kept as `k3ng_rotator_controller/k3ng_rotator_controller.ino` so it still opens in the
Arduino IDE. For Arduino IDE builds the vendored `libraries/` subfolders have to be on the IDE's library path
(copy or symlink them into your sketchbook `libraries/`); PlatformIO does **not** need this and pulls
`LiquidCrystal` from the registry instead.

There is no test suite — this is firmware. Verification means: it compiles, it fits in the flash/RAM budget, and it
behaves correctly on the physical board (GS-232 commands over serial, relay switching, position readback).

### Flash and RAM budget

The ATmega328P leaves very little headroom: a "stock" K3NG configuration with the LCD and preset encoder enabled
**overflows flash by roughly 1.3 KB**. The current configuration builds at **16600 B flash (54.0 %) / 1053 B RAM
(51.4 %)** — verified with PlatformIO Core 6.1.19 — headroom bought back by the two `OPTION_SAVE_MEMORY_EXCLUDE_*` switches (they strip rarely-used extended and `\`-prefixed serial
commands; core GS-232 rotate/query commands are untouched) and by disabling every unused subsystem.

**After enabling any new `FEATURE_*`, run `pio run` and check the reported usage before considering the change
done.**

### Vendored libraries caveat

`libraries/` holds ~15 Arduino-IDE-style third-party libraries used by *optional* upstream features. Do **not** add
that folder wholesale via `lib_extra_dirs`: PlatformIO's dependency finder scans `#include` lines textually and
ignores `#ifdef` guards, so it would pull in every library including Mega-only ones (`TimerFive`) that do not
compile for the ATmega328P. If a newly enabled feature needs one of them, add that single subfolder.

## Differences from upstream

Beyond configuration, this fork carries four code changes. `git fetch upstream` merges will not reintroduce or
update them, and button/loop changes from upstream may conflict.

1. **AZ pulse debounce (new code).** Upstream debounces only the *elevation* pulse input. Because this board's
   azimuth sensor is a bounce-prone mechanical reed switch, the same pattern was ported to azimuth:
   `OPTION_AZ_PULSE_DEBOUNCE` gates it, `AZ_POSITION_PULSE_DEBOUNCE` (20 ms) sizes it, and
   `az_position_pulse_interrupt_handler()` mirrors the EL structure. Do **not** copy the EL default of 500 ms — at
   0.5° per pulse a fast rotator produces real pulses far more often than that, and an over-long window silently
   drops genuine pulses. Keep the value well below the shortest real inter-pulse gap at full rotation speed.
2. **Non-blocking manual-button release debounce.** Upstream `check_buttons()` calls a blocking `delay(200)` on
   every jog-button release, stalling the whole loop. It now uses a `millis()`-based pending timer
   (`az_button_release_pending_time`): the stop request is issued once 200 ms have elapsed across loop iterations,
   and a re-press cancels it. Same behaviour and same window, without the stall.
3. **Trimmed main loop.** Upstream calls `read_headings()` / `service_rotation()` three times per `loop()` to keep
   rotation responsive around the potentially slow GPS / ethernet / tracking blocks. Since this fork will never
   enable those, the two extra pairs were removed; the pair at the top of `loop()` and the `read_headings()` that
   feeds `check_buttons()` / `check_overlap()` / `check_brake_release()` remain. If one of those blocks is ever
   enabled, put an equivalent pair back around it.
4. **Alternate hardware profiles removed.** Upstream's `rotator_*_<board>.h` triads (`_m0upu`, `_wb6kcn`,
   `_wb6kcn_k3ng`, `_test`) and two orphan pin files were deleted, and the `HARDWARE_*` selection mechanism in
   `rotator_hardware.h` is not used. The default `rotator_features.h` / `rotator_pins.h` / `rotator_settings.h`
   are edited directly. Keep it that way, and drop those files again if a future upstream merge reintroduces them.

## Things that must not be "fixed"

This codebase has no single rotation-capability switch — several subsystems hardcode 360°. For a 450° rotator:

- `OPTION_AZ_POSITION_PULSE_HARD_LIMIT` must stay **enabled**. Without it the pulse-counted azimuth wraps at a
  hardcoded 360° instead of clamping at `azimuth_starting_point + azimuth_rotation_capability`, corrupting position
  tracking as soon as the rotor enters the overlap zone.
- `OPTION_PRESET_ENCODER_0_360_DEGREES` must stay **disabled**. Enabled, it clamps the preset knob at a hardcoded
  360°, making the 360–450° overlap zone unreachable from the front panel.
- `OPTION_EXTERNAL_ANALOG_REFERENCE` is **disabled**, and that is correct here — but only because this build never
  performs an analog read. `analogReference(EXTERNAL)` sits inside `analogReadEnhanced()` rather than `setup()`, and
  every caller is either in a disabled feature or behind an `if (pin)` guard on a pin set to `0`. With no
  `analogRead()` ever executed, `ADMUX` stays at its reset default (`REFS=00`, i.e. the AREF pin with the internal
  reference off) — already the right state for the board's own reference. **If you ever enable a feature that reads
  an analog pin** (position potentiometer, speed pot, preset pot, joystick), re-enable this option in the same
  change: the first `analogRead()` would otherwise apply whatever reference the Arduino core defaults to and connect
  AVCC internally to the AREF pin, which is being driven externally.
- `OPTION_DISPLAY_HEADING` must stay **disabled**, with only `OPTION_DISPLAY_HEADING_AZ_ONLY` on. With elevation
  off, the generic option also prints the azimuth heading (to row 2), duplicating row 1 and wasting the second LCD
  row. Both were enabled at once by upstream default; that was fixed.

## Repository layout

```
k3ng_rotator_controller/     the sketch (.ino) plus rotator_*.h/.cpp, all one translation unit
  rotator_features.h         what is compiled in
  rotator_pins.h             pin assignments
  rotator_settings.h         tunable values, calibration, limits
  rotator_dependencies.h     compile-time rule checker (#errors on invalid combinations)
libraries/                   vendored third-party libraries for optional upstream features
tle/                         upstream satellite TLE data (unused by this build)
platformio.ini               PlatformIO configuration
CLAUDE.md                    working notes for Claude Code
```

There is no OOP layering: `setup()` initializes serial, EEPROM settings, pins, display, encoders and interrupts,
and `loop()` is a flat polling sequence of `check_*()` / `service_*()` / `read_*()` calls gated by `#ifdef`.
Interrupts are used only for the encoder and the position pulse input.

## License

Upstream K3NG licensing applies — see [LICENSE](LICENSE).
