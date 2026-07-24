# K3NG Rotator Controller — RemoteQTH v3.3 / Nano "light" fork

A trimmed-down fork of the [K3NG Arduino rotator controller](https://github.com/k3ng/k3ng_rotator_controller),
configured for exactly one hardware setup: a **RemoteQTH Rotator Interface v3.3** board (Arduino Nano, ATmega328P,
old bootloader) driving an **azimuth-only, 405°-capable** rotator whose position sensor is a **reed switch /
contactron pulse output**.

This is not a general-purpose build. Every optional K3NG subsystem that this board does not use (elevation, GPS,
RTC/clock, ethernet, moon/sun/satellite tracking, stepper motor, compass/accelerometer sensors, alternate display
types) is disabled, and the alternate-hardware profile files shipped upstream have been removed — the ATmega328P
does not have the flash to carry them.

Upstream's general project description, protocol documentation and feature list still apply and live in the
[K3NG wiki](https://github.com/k3ng/k3ng_rotator_controller/wiki).

A companion project, [sq9fk/rotator_wifi_bridge](https://github.com/sq9fk/rotator_wifi_bridge), puts this controller
on the network over rotctld, a raw GS-232 socket and a web panel. The `I` and `D` commands below were added for it.

## Hardware

| Item | Value |
|---|---|
| Board | RemoteQTH Rotator Interface v3.3 (OK1HRA), Arduino Nano ATmega328P, old bootloader |
| Flash / RAM | 30720 B / 2048 B |
| Axes | Azimuth only (`FEATURE_ELEVATION_CONTROL` off) |
| Rotator | 405° rotation capability (360 + 45° overlap), starting point 180° |
| Position sensor | Pulse input (dry contact reed switch), 0.5° per pulse |
| Display | 16×2 HD44780 LCD, 4-bit parallel |
| Controls | Manual CW/CCW jog buttons + rotary encoder for azimuth preset |
| Control protocol | Yaesu GS-232B emulation over USB serial, 9600 baud |

### Pin map (traced from the rev 3.3 KiCad schematic, plus later physical rewiring)

| Signal | Pin | Notes |
|---|---|---|
| Motor drive CW | D6 | → MC33186 **IN1** (see H-bridge note below) |
| Motor drive CCW | D7 | → MC33186 **IN2** |
| Bridge enable | D8 | → MC33186 **D2** (active-HIGH disable); held LOW = bridge always enabled |
| Azimuth position pulse | D2 (INT0) | interrupt-capable pin required; internal pull-up enabled |
| Manual CW / CCW buttons | A5 / A4 | moved off A2/A3 to free A2 for the LCD |
| AZ preset rotary encoder | D10 (CW) / D9 (CCW) | half-step mode, pull-ups enabled |
| Rotation stall indicator | D13 | onboard Nano LED, no external wiring; goes high when a stall is detected |
| LCD RS / E | D12 / D11 | |
| LCD D4 / D5 / D6 / D7 | D5 / D4 / D3 / **A2** | LCD D7 moved off D2 to free D2 for the position pulse input |
| AREF | external | supplied by the board through R9; no software setting needed, see below |

Unused K3NG outputs (PWM speed control, speed/preset pots, overlap LED, serial LED, stop button) are set to `0`
(disabled) in `rotator_pins.h`.

### Motor drive: MC33186 H-bridge, not relays

The board's relays and their driver transistors (Q1–Q3) were removed. An **MC33186 H-bridge module (CJMCU-3386)**
now drives the motor, fed from the old transistor-base nodes through a **non-inverting level shifter**:

- D6 (`rotate_cw`) → **IN1**, D7 (`rotate_ccw`) → **IN2**. The MC33186 is non-inverting (input high → that output
  high), so the drive stays active-HIGH — no polarity change. `rotate_cw` high drives one way, `rotate_ccw` high the
  other, and **both low shorts the motor to ground, dynamically braking it** — that state holds the rotator at rest,
  taking over the job the mechanical brake relay used to do.
- D8 (`brake_az`) → the bridge's **D2** disable input (active-HIGH: high = outputs high-Z/coast, low = enabled).
  `BRAKE_ACTIVE_STATE`/`INACTIVE_STATE` are both forced **LOW** so D2 stays low and the bridge is always enabled,
  and `AZ_BRAKE_DELAY` is `0`. Stock K3NG raises `brake_az` to "release" the brake for rotation, which here would
  raise D2 and disable the bridge exactly when trying to move — so the polarity had to be inverted.

**Not yet verified on the physical setup.** Two things to check on the bench: (1) if the rotator will not move at
all, D2 is active-LOW on this module — set both brake states HIGH instead; (2) if it moves the wrong way, swap the
motor leads on OUT1/OUT2 (a swap, not a polarity flip). Because the H-bridge shorts the motor when both inputs are
low, there is no vertical shoot-through even if CW and CCW were ever asserted together.

Drive is plain on/off (full speed). PWM speed control (soft start/stop) would suit the H-bridge but is blocked by
the pin budget — all six ATmega328P PWM pins are used by the LCD and preset encoder. Recorded as a potential future
feature in [CLAUDE.md](CLAUDE.md).

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
OPTION_AZ_POSITION_PULSE_HARD_LIMIT  required past 360 deg (see below)
OPTION_AZ_PULSE_DEBOUNCE             fork-added, not in upstream (see below)
OPTION_AZ_MANUAL_ROTATE_LIMITS       jog buttons stop at the software end stops
FEATURE_AZ_ROTATION_STALL_DETECTION  cuts the motor if position stops changing mid-rotation
OPTION_ROTATION_STALL_DETECTION_SERIAL_MESSAGE
OPTION_DISPLAY_STATUS                LCD row 1 status
OPTION_DISPLAY_HEADING_AZ_ONLY       LCD row 1 heading
OPTION_SAVE_MEMORY_EXCLUDE_EXTENDED_COMMANDS
OPTION_SAVE_MEMORY_EXCLUDE_BACKSLASH_CMDS
OPTION_LOCK_AZIMUTH_CONFIGURATION    fork-added, not in upstream (see below)
LANGUAGE_ENGLISH
```

## Serial command set

GS-232B over the USB port, 9600 8N1. The memory-saving options above strip most of K3NG's command surface, so what
actually answers is a short list:

| Command | Action | Reply |
|---|---|---|
| `C` | report azimuth (real, 0–359 — see `I` for raw) | `AZ=xxx` |
| `C2` | report azimuth and elevation | `AZ=xxxEL=000` — the elevation is a dummy, there is no EL axis |
| `M###` | rotate to **raw** azimuth 180–585 (values below 180 clamp to the CCW stop) | nothing on success, `?>` if over 585 |
| `L` / `R` | rotate CCW / CW | — |
| `A` | stop azimuth rotation | — |
| `S` | stop everything | — |
| `X1`–`X4` | speed change | `Speed X1`…`X4` — **accepted but inert**, this board has no PWM speed output wired |
| `I` | report **raw** azimuth (180–585) | `RAW=370` |
| `Ixxx` | declare the rotator's true raw position and save to EEPROM | `RAW=370`, or `Wait` inside the post-boot lockout |
| `D` | report degrees per position pulse | `DPP=0.500` |
| `Dxxxx` | set degrees per pulse to `xxxx`/1000 and save to EEPROM immediately | `DPP=0.500`, or `Wait` inside the post-boot lockout |
| `H` | help | nothing (`OPTION_SERIAL_HELP_TEXT` is off) |

Everything else answers `?>`. Specifically not available: the elevation commands (`B`, `W`, `U`, `D`, `E`), the
potentiometer calibration commands (`F`, `F2`, `O`, `O2`), the timed-buffer commands (`N`, `T`, multi-point `M`),
`P36`/`P45` and `Z` (see `OPTION_LOCK_AZIMUTH_CONFIGURATION` below), all ~20 backslash commands (`\A`, `\I`, `\J`,
`\E`, `\Q`, …) and all ~50 extended `\?XX` commands.

**Rotational and configuration commands are silently ignored for the first 5 s after boot**
(`ROTATIONAL_AND_CONFIGURATION_CMD_IGNORE_TIME_MS`, in effect because
`OPTION_ALLOW_ROTATIONAL_AND_CONFIGURATION_CMDS_AT_BOOT_UP` is *disabled* — the option's name reads backwards). No
error is returned, the command is just dropped. Opening the USB port resets the Nano, so a control program that
sends a command immediately on connect will lose that first one.

### Key settings (`rotator_settings.h`)

| Setting | Value |
|---|---|
| `AZIMUTH_STARTING_POINT_EEPROM_INITIALIZE` | 180 |
| `AZIMUTH_ROTATION_CAPABILITY_EEPROM_INITIALIZE` | 405 |
| `AZ_POSITION_PULSE_DEG_PER_PULSE` | 0.5 — EEPROM seed only; the live value is calibrated with the `D` command |
| `AZ_POSITION_PULSE_DEBOUNCE` | 20 ms |
| `AZIMUTH_TOLERANCE` | 3.0° |
| `AZ_MANUAL_ROTATE_CCW_LIMIT` / `CW_LIMIT` | 182 / 583 (raw azimuth, 2° inside the mechanical stops) |
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
**overflows flash by roughly 1.3 KB**. The current configuration builds at **16922 B flash (55.1 %) / 1063 B RAM
(51.9 %)** — verified with PlatformIO Core 6.1.19 — headroom bought back by the two `OPTION_SAVE_MEMORY_EXCLUDE_*` switches (they strip rarely-used extended and `\`-prefixed serial
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
4. **Runtime-calibratable degrees-per-pulse (new code).** Upstream treats `AZ_POSITION_PULSE_DEG_PER_PULSE` as a
   compile-time constant with no serial command and no EEPROM storage. Here `config_t` gained an
   `az_position_pulse_deg_per_pulse` float (seeded from the `#define` when EEPROM is initialized), the interrupt
   handler reads that field instead of the constant, and the `D` command reports or sets it — writing to EEPROM
   immediately. The store is wrapped in `noInterrupts()`/`interrupts()` because the pulse ISR reads the same float
   and a 4-byte store is not atomic on AVR. `CONFIGURATION_STRUCT_VERSION` was bumped 123 → 124 so an EEPROM image
   written by an older build is discarded rather than misread — **the first boot after flashing this therefore
   re-initializes EEPROM and loses the stored azimuth**, so set the position once afterwards.
5. **Raw position query and sync, the `I` command (new code).** `C` reports a real azimuth of 0–359, which cannot
   express which turn the rotator is on — 10° is raw 10 or raw 370 and nothing in the protocol distinguishes them.
   `I` reports the raw value; `Ixxx` declares the rotator's true raw position, for when the count has drifted or the
   rotator was moved by hand. `I` is not part of the GS-232 command set, so no logging program will send it by
   accident. The setter writes `az_position_pulse_input_azimuth` — the accumulator the pulse ISR increments and
   `read_azimuth()` derives from — under `noInterrupts()`. **Upstream's `\A` does not do this**: it sets
   `raw_azimuth` and `last_azimuth` only, so on a pulse-input rotator its effect is undone by the next pulse.
6. **`OPTION_LOCK_AZIMUTH_CONFIGURATION` (new option).** GS-232B's `P36`/`P45` set the rotation capability and `Z`
   toggles the starting point between north- and south-centre. On this rotator both are destructive: the 180°
   starting point and 405° capability are the basis for the jog limits, the pulse hard limit and the preset encoder,
   and a logging program that issues `P36` on connect would desync all three until the next reset. Since `\I` and
   `\J` — the proper way to change those values — are stripped anyway, the two cases are now compiled out and answer
   `?>`. Saves 246 B flash.
7. **`EEPROM.update()` in `write_settings_to_eeprom()`.** Upstream writes the whole ~58-byte configuration struct
   unconditionally. `check_for_dirty_configuration()` calls it every 30 s whenever anything is dirty — including the
   routine `last_azimuth` update after a rotation — so each save burned 58 cells of the 100k-cycle EEPROM and stalled
   `loop()` for ~190 ms (3.3 ms per cell) to persist four changed bytes. `update()` skips unchanged bytes.
8. **Alternate hardware profiles removed.** Upstream's `rotator_*_<board>.h` triads (`_m0upu`, `_wb6kcn`,
   `_wb6kcn_k3ng`, `_test`) and two orphan pin files were deleted, and the `HARDWARE_*` selection mechanism in
   `rotator_hardware.h` is not used. The default `rotator_features.h` / `rotator_pins.h` / `rotator_settings.h`
   are edited directly. Keep it that way, and drop those files again if a future upstream merge reintroduces them.

## Things that must not be "fixed"

This codebase has no single rotation-capability switch — several subsystems hardcode 360°. For this 405° rotator:

- `OPTION_AZ_POSITION_PULSE_HARD_LIMIT` must stay **enabled**. Without it the pulse-counted azimuth wraps at a
  hardcoded 360° instead of clamping at `azimuth_starting_point + azimuth_rotation_capability`, corrupting position
  tracking as soon as the rotor enters the overlap zone.
- `OPTION_PRESET_ENCODER_0_360_DEGREES` must stay **disabled**. Enabled, it clamps the preset knob at a hardcoded
  360°, making the 360–405° overlap zone unreachable from the front panel.
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
