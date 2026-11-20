# pic-hack-solrbrite

Replacement firmware for the controller of the **Costco #1806387 Fresh Source /
Sol-R-Brite 5000 lm motion solar area light** (model FSI 80-8572-M, UPC 850049867979).

The stock controller has three motion brightness modes (1000 / 2000 / 5000 lm).
In every mode it also keeps a dim ~50 lm glow on all night, and that can't be
turned off. This project replaces the unmarked, one-time-programmable 8-pin MCU
(U1) with a **PIC12LF1572** running custom firmware:

- **Motion-only by default.** The night glow is now optional, set by a switch added on the spare pin 5.
- The same three motion levels, cycled with the original button (1 → 2 → 3 → OFF).
- Gradual fade on and off, low-battery cutoff, PIR warm-up and lockout.
- The rest of the board is unchanged: charger, battery protection, LED drivers, PIR, light sensor.

> Status: **firmware skeleton, not built or tested yet.** Every timing and
> threshold constant is a placeholder until it is measured on the stock
> controller (see [Calibration](#calibration)).

### Who this is for

Anyone with basic DIY electronics skills: you can solder, use a multimeter and
follow a wiring table. No PIC or embedded C experience is needed. Everything
uses free tools; the only purchases are a few cheap parts.

### What you need

**Parts to buy**

| Part                                       | Part number                                               | What it's for                                                                      | Rough price | Where                                                                                                    |
| ------------------------------------------ | --------------------------------------------------------- | ---------------------------------------------------------------------------------- | ----------- | -------------------------------------------------------------------------------------------------------- |
| PIC12LF1572, DIP-8 (through-hole)          | Microchip PIC12LF1572-I/P                                 | Breadboard tests and the development socket                                        | $1          | [DigiKey](https://www.digikey.com/en/products/detail/microchip-technology/PIC12LF1572-I-P/4739321)       |
| PIC12LF1572, SOIC-8 (SMD)                  | Microchip PIC12LF1572-I/SN                                | Final build, soldered onto the U1 pads                                             | $1          | [DigiKey](https://www.digikey.com/en/products/detail/microchip-technology/PIC12LF1572-I-SN/4739322)      |
| DIP-8 IC socket                            | TE Connectivity 1-2199298-2                               | Development socket wired to the U1 pads ([section 5](#development-hardware-setup)) | under $1    | [DigiKey](https://www.digikey.com/en/products/detail/te-connectivity-amp-connectors/1-2199298-2/5022039) |
| SOIC-8 to DIP-8 clamshell adapter, 150 mil | HiLetgo SOIC8 SOP8 to DIP8 EZ Programmer Adapter (2-pack) | Holds the SOIC chip in the programmer wiring so you can flash it before soldering  | $9 for 2    | [Amazon](https://www.amazon.com/dp/B01HTC5DKS)                                                           |

Buy a spare chip of each kind; they're cheap, and a spare saves a reorder if one gets damaged.

**Also needed**

- **Programmer:** a Raspberry Pi 3B+ or 4B you already own, plus jumper wires. If you don't have a Pi, the fallback is a Microchip **MPLAB Snap** (PG164100), roughly $35–40.
- **Optional, only for the HV build** (button without resets, [2.1](#21-the-button-and-the-two-builds)): a high-voltage programmer, such as a PICkit 3 clone ($25–35) or a PICkit 5 ($77). See [Flashing the HV build](#flashing-the-hv-build).
- **Tools:** soldering iron, hot air or ChipQuik to remove the old chip, multimeter. A scope is useful for calibration but not required to get going.
- **Software (free):** VS Code, the MPLAB extension pack, the MPLAB XC8 compiler in free mode, and Pickle on the Pi. `tools\setup.ps1` installs the Windows side.

### Other docs
- [TESTING.md](TESTING.md): how to check the firmware on a breadboard and in the light.
- [DECISIONS.md](DECISIONS.md): why the design is the way it is, and the alternatives that were ruled out.
- [AGENTS.md](AGENTS.md): project rules for contributors (tools, doc and code style, code/docs/tests in lockstep).

Circuits are drawn as text. Timing diagrams are [WaveDrom](https://wavedrom.com/) blocks: open the Markdown preview in VS Code (Ctrl+Shift+V) with the extension `tools\setup.ps1` installs to see them drawn. GitHub shows their source instead.

---

## 1. Original hardware (as found)

### 1.1 Product
| Item           | Value                                                                               |
| -------------- | ----------------------------------------------------------------------------------- |
| Retail         | Costco item 1806387 (US), also sold as Sol-R-Brite (Costco AU 1806387, Walmart)     |
| Model          | Fresh Source FSI 80-8572-M                                                          |
| UPC            | 850049867979                                                                        |
| Rated output   | 1000 / 2000 / 5000 lm on motion, 50 lm steady glow when idle at night; 120° PIR     |
| Stock behavior | Button: 3 modes + off. In every mode: 50 lm glow dusk to dawn, full level on motion |

### 1.2 Power path

```text
 S+ (solar panel)
      |
 [U5 charger, Q6/Q7]
      |
      +------------------------+-----------------------+----------- B+
      |                        |                       |
 [4 x 18650, 1S4P]       [U2 regulator]                L+
  (all B+ tied)                |                  [LED array]
   |           |          VCC 2.5 V to U1,             L-
  B- pair 1   B- pair 2   PIR, light sensor            |
   |           |                                   [Q1..Q5] <---- gates: U1 pin 6
 [U3]        [U4]    XB8886A protection ICs            |
   |           |                                       |
   +-----------+---------------------------------------+----------- GND (board)
```

- **Battery:** 4 × 18650 Li-ion, **1S4P**, about 3.7 V nominal. All B+ wires are tied together. The four B− wires go in pairs to the two protection ICs.
- **Protection:** U3 and U4 are **XB8886A** (XySemi), single-cell Li-ion protection ICs with built-in MOSFETs (overcharge, over-discharge, overcurrent, short circuit). There is one per cell pair. Their protected outputs join to form board GND.
- **Charger:** U5, marked **XC3101-5x** (not identified; it works independently of the MCU). It sits with Q6/Q7 near the solar input (S+). The MCU has no control over charging.
- **LED supply:** L+ is tied straight to B+. The LEDs are switched on the low side (L−) by **Q1–Q5**, MOSFETs in parallel.
- **Logic supply:** U2 is a 3-pin regulator surrounded by caps. It makes **VCC = 2.5 V** for U1, the PIR and the light sensor.

### 1.3 Other parts
- **PIR:** 3-pin digital pyroelectric sensor with built-in signal processing. Pins: a = output (to U1 pin 2), b = VCC, c = GND. It looks down.
- **Light sensor ("CDS" on the silkscreen):** a white, flat 2-pin SMD part on the back of the board. It looks through a hole in the PCB and out the PIR window. Measured: about 10–12 kΩ forward when lit, open when dark. Reverse reads 21 kΩ whether lit or dark, probably through the circuit around it (U1's protection diode) rather than the part. Behaves like an **NPN phototransistor** (PT19-21C / ALS-PT19 class): collector to VCC, emitter to U1 pin 3. **This is the dusk/dawn sensor**; the solar panel isn't wired to U1.
- **Button:** pin 4 to GND.

### 1.4 U1, the original MCU
- **Package:** SOIC-8 narrow (pin pitch 1.27 mm, 3.9 mm body). Unmarked.
- **Pin layout:** VDD on pin 1, GND on pin 8, input-only pin 4. This is the PIC12F-style layout that many Chinese one-time-programmable MCUs copy (Padauk PMS15x, Sinomcu MC30P60xx, FMD FT60F01x, ...). Assuming the firmware can't be read out.

| Pin | Net           | Components                                                   |
| --- | ------------- | ------------------------------------------------------------ |
| 1   | VCC 2.5 V     | U2 output                                                    |
| 2   | PIR.a         | PIR digital output                                           |
| 3   | Light sense   | phototransistor to VCC; **R1 330 kΩ** ‖ C3 to GND            |
| 4   | Button        | push button to GND                                           |
| 5   | not connected |                                                              |
| 6   | LED drive     | **R9 620 Ω** to Q1–Q5 gates; **R8 100 kΩ** from gates to GND |
| 7   | Battery sense | **R3 1.2 MΩ** from B+; **R2 620 kΩ** ‖ C4 to GND             |
| 8   | GND           |                                                              |

The same circuits, drawn pin by pin:

```text
 U1 pin 1 ---- VCC 2.5 V (U2 output)              U1 pin 8 ---- GND

 U1 pin 2 ---- PIR output "a"      (PIR "b" = VCC, "c" = GND)

                  VCC 2.5 V
                      |
                      C
             light -> phototransistor ("CDS", back of the board)
                      E
                      |
 U1 pin 3 ------------+
                      |
                  +---+---+
                  |       |
                [R1]    [C3]
                330k      |
                  |       |
                  +---+---+
                      |
                     GND

 U1 pin 4 ----[button]---- GND

 U1 pin 5 ---- (not connected)

 U1 pin 6 ----[R9 620R]----+----> gates of Q1..Q5
                           |
                         [R8]
                         100k
                           |
                          GND

 B+ ----[R3 1.2M]----+---- U1 pin 7
                     |
                 +---+---+
                 |       |
               [R2]    [C4]
               620k      |
                 |       |
                 +---+---+
                     |
                    GND
```

### 1.5 Resistor codes
| Ref | Code | Value  | Role                                                           |
| --- | ---- | ------ | -------------------------------------------------------------- |
| R1  | 3303 | 330 kΩ | Light sensor load (pin 3 to GND)                               |
| R2  | 6203 | 620 kΩ | Battery divider, bottom leg                                    |
| R3  | 1204 | 1.2 MΩ | Battery divider, top leg                                       |
| R8  | 104  | 100 kΩ | Gate pull-down (LEDs stay off while U1 is in reset or missing) |
| R9  | 6200 | 620 Ω  | Gate series resistor                                           |

### 1.6 Useful numbers
The circuits these numbers come from are drawn in [1.4](#14-u1-the-original-mcu).
- **Battery divider:** 620k / 1.82M = **0.341**. At 4.2 V the pin sees 1.43 V; at 3.0 V, 1.02 V. Source impedance is about 410 kΩ, and C4 holds the voltage up during ADC sampling. The divider draws about 2.3 µA.
- **Light sense:** pin 3 ≈ photocurrent × 330 kΩ. That's about 0 V in the dark, 0.33 V at 1 µA, and near VCC in daylight. Leakage in the dark rises with temperature (tens of mV on a hot day).
- **Gate drive:** 620 Ω into five gates (about 5 nF total, estimated) gives a time constant of about 3 µs. Switching is slow, so keep the PWM frequency low, close to the stock value.
- **Night glow cost (estimated):** about 0.4 W for 12 h is about 5 Wh per night, more than 10% of the pack's roughly 40 Wh. That's the main reason for this project.

---

## 2. Replacement MCU: PIC12LF1572

Datasheet: PIC12(L)F1571/2, DS40001723, free from the [Microchip product page](https://www.microchip.com/en-us/product/PIC12LF1572) (Documentation tab). It is not included in this repo; if you keep a local copy, put it in `refs/` (git-ignored). The LF version runs on 1.8–3.6 V, so 2.5 V VCC is fine. It has 16-bit PWMs and high-endurance flash (HEF) for settings.

| Pin | PIC function              | Use in this firmware                                                   |
| --- | ------------------------- | ---------------------------------------------------------------------- |
| 1   | VDD                       | 2.5 V                                                                  |
| 2   | RA5 (interrupt-on-change) | PIR input; a rising edge wakes the chip from Sleep                     |
| 3   | RA4 / AN3                 | Light level (ADC)                                                      |
| 4   | RA3 / MCLR                | Button: **MCLR reset** in the LVP build, normal input in the HV build (see below) |
| 5   | RA2                       | **Night-light switch** to GND, weak pull-up enabled only while reading |
| 6   | RA1 / PWM1                | LED PWM (APFCON.P1SEL = 0)                                             |
| 7   | RA0 / AN0                 | Battery voltage (ADC)                                                  |
| 8   | VSS                       | GND                                                                    |

### 2.1 The button and the two builds

Two terms first:
- **MCLR** is the PIC's reset pin. Pulling it low resets the chip.
- **LVP (low-voltage programming)** lets a programmer write the chip at normal logic levels. Without LVP, entering programming mode needs about 8–9 V on MCLR: **high-voltage (HV) programming**, which only some programmers can do.

While LVP is enabled, the PIC forces pin 4 to be MCLR, and the stock button is wired to pin 4. So the firmware comes in two builds, chosen by `HV_PROGRAMMING_ONLY` in `src/app_config.h` (background in [DECISIONS.md](DECISIONS.md) D1):

|                             | LVP build (default)                                                  | HV build (`HV_PROGRAMMING_ONLY = 1`)                                     |
| --------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| Can program it              | Raspberry Pi + Pickle, MPLAB Snap, or an HV programmer               | Only an HV programmer, both to write it and to replace it later ([section 5](#flashing-the-hv-build)) |
| Pin 4                       | MCLR: a press holds the chip in reset                                | RA3, a normal input with the internal pull-up                            |
| When the mode changes       | On release: the chip boots, sees the MCLR-reset flag, advances       | On release, read as an input                                             |
| Side effects of a press     | LEDs go dark while it's held; a running motion hold is cancelled; day/night is re-read from one sample | None: the light keeps running                                            |
| Pressed while asleep        | Resets the chip                                                      | Wakes the chip (interrupt-on-change on RA3)                              |
| Output file                 | `out\cli\solrbrite.hex`                                              | `out\cli\solrbrite-hv.hex`                                               |

> **The Pi and the Snap can't write the HV build.** The chip refuses to clear LVP during low-voltage programming, so you'd get the HV firmware with pin 4 still MCLR: a button that does nothing. `tools\flash-pi.ps1` refuses a HEX with LVP off. Once an HV programmer has written the HV build, only an HV programmer can program that chip again, including to put the LVP build back.

Pin 4 in each build, and the switch added on pin 5 (both builds):

```text
 LVP build                                HV build

   VDD                                      VDD
    |                                        |
 internal pull-up (always on)             internal pull-up (WPUA3)
    |                                        |
 pin 4 MCLR ---[button]--- GND            pin 4 RA3 ---[button]--- GND
 press:   chip held in reset              press:   input reads low; wakes the chip
 release: chip boots, mode + 1            release: mode + 1


 Both builds, added for the night light:

   VDD
    |
 internal pull-up, on only while the firmware reads the pin
    |
 pin 5 RA2 ---[switch]--- GND             closed = night light on
```

A press in each build. The diagrams show contact bounce (the button chattering for a few ms) and how `BUTTON_DEBOUNCE_MS` filters it:

```wavedrom
{ "signal": [
    { "name": "pin 4 (button)", "wave": "1.0....101.........." },
    { "name": "MCU", "wave": "2.3....434..5....2..",
      "data": ["running", "reset", "boot", "reset", "boot, wait", "preview", "running"] },
    { "name": "LEDs", "wave": "2.3.........4....2..",
      "data": ["as before", "off (R8 holds the gates low)", "new mode level", "normal"] },
    { "node": ".........A..B....C.." }
  ],
  "edge": ["A<->B BUTTON_DEBOUNCE_MS", "B<->C PREVIEW_S"],
  "head": { "text": "LVP build: a press is an MCLR reset" }
}
```

```wavedrom
{ "signal": [
    { "name": "pin 4 (button)", "wave": "1.010......101......" },
    { "name": "debounced level", "wave": "1......0........1..." },
    { "name": "mode", "wave": "2...............3...", "data": ["n", "n + 1, preview"] },
    { "node": "....A..B.....C..D..." }
  ],
  "edge": ["A<->B BUTTON_DEBOUNCE_MS", "C<->D BUTTON_DEBOUNCE_MS"],
  "head": { "text": "HV build: pin 4 is an input, no reset" }
}
```

---

## 3. Firmware behavior

| Situation                                   | LEDs                                                                                                                                            |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| Day (light above `LIGHT_DAY_MV`, confirmed) | Off. The MCU sleeps and wakes every ~2 s on the watchdog to sample light, battery and the switch                                                |
| Night, idle, pin-5 switch **open**          | Off; sleeping, woken by PIR edges                                                                                                               |
| Night, idle, pin-5 switch **closed**        | Level 0 (~50 lm night light). Briefly blanked every `NIGHTLIGHT_SENSE_S` to check for dawn                                                      |
| Night, motion                               | Fade up to the mode level (1/2/3) and hold for `MOTION_HOLD_S` after the last motion, then fade back to idle                                    |
| Button press                                | Mode 1 → 2 → 3 → OFF → 1 on release, saved in HEF. The new mode's brightness shows for `PREVIEW_S` (N = 3 s), then normal operation. OFF = no light at all. In the LVP build the press is a reset, so the LEDs are dark while it's held ([2.1](#21-the-button-and-the-two-builds)) |
| Battery below `BATT_CUTOFF_MV`              | All LEDs disabled until the battery recovers above `BATT_RESUME_MV`                                                                             |
| After power-on                              | PIR ignored for `PIR_WARMUP_S`                                                                                                                  |
| After the light goes off                    | PIR ignored for the fade-off time plus `PIR_LOCKOUT_MS`, so the supply transient doesn't retrigger it                                           |

Motion at night. "Idle" is off, or the night light when the pin-5 switch is closed. A new motion during the hold restarts `MOTION_HOLD_S`, counted from when the PIR output falls:

```wavedrom
{ "signal": [
    { "name": "PIR output", "wave": "010..10..........." },
    { "name": "LEDs", "wave": "23.4.......5.2....",
      "data": ["idle", "fade up", "mode level", "fade down", "idle"] },
    { "name": "PIR accepted", "wave": "1..........0....1." },
    { "node": ".A.B..C....D.E..F." }
  ],
  "edge": ["A<->B FADE_ON_MS", "C<->D MOTION_HOLD_S", "D<->E FADE_OFF_MS", "E<->F PIR_LOCKOUT_MS"],
  "head": { "text": "Motion at night" }
}
```

Light and battery are only sampled when the LEDs have been fully off for `LIGHT_SETTLE_MS`. That keeps the sensor from seeing the light's own LEDs (it looks down at the lit ground) and avoids battery sag under load. With the night light on, the LEDs are blanked for one sample every `NIGHTLIGHT_SENSE_S`:

```wavedrom
{ "signal": [
    { "name": "LEDs", "wave": "3...2..3......2..3..",
      "data": ["night light", "off", "night light", "off", "night light"] },
    { "name": "light sample", "wave": "0.....10........10.." },
    { "node": "....A.........C....." },
    { "node": "....D.E............." }
  ],
  "edge": ["A<->C NIGHTLIGHT_SENSE_S", "D<->E LIGHT_SETTLE_MS"],
  "head": { "text": "Night light: blanking to check for dawn" }
}
```

### Source layout
```
src/
  app_config.h   all tunable constants (timings, PWM, thresholds)  <- edit here
  board.h        pin map, register bit masks, HEF address
  config_bits.c  configuration words (LVP = ON!)
  hal.c/.h       clock, tick, PWM, ADC, inputs, sleep, HEF
  fade.c/.h      linear fades with fixed-point stepping
  main.c         state machine
```
No interrupt handler is used. The main loop polls a 10 ms Timer2 tick. Sleep is woken by the watchdog or by interrupt-on-change with GIE=0. The mode is stored in HEF at 0x780 (low byte) plus an inverted copy for validation. The build keeps 0x780–0x7FF free (`-mrom=default,-780-7ff`).

---

## 4. Calibration

Measure these on the **stock** controller before removing U1 (scope ground at Q1–Q5 sources / board GND):

| Constant(s)                      | What to measure                                                                    | Where        |
| -------------------------------- | ---------------------------------------------------------------------------------- | ------------ |
| `PWM_PERIOD_US`                  | PWM period                                                                         | U1 pin 6     |
| `LEVEL0..3_HIGH_US`              | High time for the glow and for each mode on motion                                 | U1 pin 6     |
| `FADE_ON_MS`, `FADE_OFF_MS`      | Stock ramp times, if any (or choose your own)                                      | U1 pin 6     |
| `MOTION_HOLD_S`                  | Time on after the last motion                                                      | U1 pin 6     |
| `LIGHT_NIGHT_MV`, `LIGHT_DAY_MV` | Pin 3 voltage at the moment the stock unit switches at dusk and at dawn            | U1 pin 3     |
| `LIGHT_SETTLE_MS`                | Pin 3 recovery time after a light shining on it is removed (330k × C3)             | U1 pin 3     |
| PIR behavior                     | Idle level, pulse width on motion, behavior during the first minute after power-up | U1 pin 2     |
| `BATT_*`                         | Check pin 7 ≈ 0.341 × B+                                                           | U1 pin 7     |
| Sleep current                    | Board current in daytime, as the baseline to beat                                  | battery lead |

Also worth checking: whether the stock firmware samples pin 3 during PWM off-time while the glow is on.

The PWM measurement on pin 6, one reading per brightness level:

```wavedrom
{ "signal": [
    { "name": "U1 pin 6", "wave": "1.0.....1.0.....1.0....." },
    { "node": "a.b" },
    { "node": "c.......d" }
  ],
  "edge": ["a<->b high time: LEVELn_HIGH_US", "c<->d period: PWM_PERIOD_US"],
  "head": { "text": "What to measure on U1 pin 6" }
}
```

---

## 5. Toolchain setup (Windows)

Run once (VS Code: **Terminal → Run Task → Setup: install toolchain**):
```powershell
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
```
It installs:
1. **VS Code extensions:** `Microchip.mplab-extension-pack`, the PowerShell extension, and `bmpenuelas.markdown-preview-wavedrom`, which draws the timing diagrams in VS Code's Markdown preview (all listed in `.vscode/extensions.json`).
2. **MPLAB XC8 compiler, free mode,** via winget (`Microchip.MPLABXC8CCompiler`, currently v3.00). For a newer release, pass the installer URL from microchip.com/xc8: `tools\setup.ps1 -Xc8InstallerUrl <url>`.
3. **PIC12-16F1xxx_DFP 1.9.258**, downloaded from packs.download.microchip.com into `tools\packs\`. Only the command-line build uses it; the VS Code extension manages its own packs.

### Building
- **VS Code / MPLAB extension:** open the folder. The project file is `.vscode/pic-hack-solrbrite.mplab.json` (device PIC12LF1572, XC8, -O2, HEF range kept free). Build with the MPLAB build command.
- **Command line** (default build task, Ctrl+Shift+B):
  ```powershell
  tools\build.ps1
  ```
  Output: `out\cli\solrbrite.hex` (plus .elf and .map).

### Flashing from the Raspberry Pi (Pickle, LVP)
1. On the Pi, run `tools/pi/install-pickle.sh`. It builds Pickle and writes `~/.pickle` (GPIO: VPP=4, PGC=25, PGD=24, BCM numbering).
2. Wiring, PIC powered from the Pi at 3.3 V. Program at 3.3 V: bulk erase needs VDD of at least 2.7 V (datasheet D112), more than the board's 2.5 V.

   | Pi physical pin | Signal   | PIC12LF1572 DIP pin    |
   | --------------- | -------- | ---------------------- |
   | 1               | 3V3      | 1 VDD (+100 nF to GND) |
   | 6               | GND      | 8 VSS                  |
   | 7 (GPIO4)       | VPP/MCLR | 4                      |
   | 22 (GPIO25)     | PGC      | 6                      |
   | 18 (GPIO24)     | PGD      | 7                      |

   ```text
                                 +----\/----+
     Pi pin 1 (3V3) -------- 1 --| VDD  VSS |-- 8 -------- Pi pin 6 (GND)
                             2 --|          |-- 7 -------- Pi pin 18 (GPIO24, PGD)
                             3 --|          |-- 6 -------- Pi pin 22 (GPIO25, PGC)
     Pi pin 7 (GPIO4, VPP) - 4 --| MCLR     |-- 5
                                 +----------+
                          PIC12LF1572, DIP-8, top view
                          100 nF ceramic capacitor from pin 1 to pin 8
   ```
3. From Windows: **Run Task → Flash via Raspberry Pi**, or
   ```powershell
   tools\flash-pi.ps1 -PiHost pi@raspberrypi.local
   ```
   (needs SSH key login). It copies the HEX to the Pi and runs `p14 lvp program`, then `p14 lvp verify`.

### Flashing with an MPLAB Snap (fallback)
If you don't have a Pi, an MPLAB Snap (PG164100) works over USB from Windows. Use the same five chip pins as the Pi wiring (Snap pin 1 = MCLR, 2 = VDD, 3 = GND, 4 = PGD, 5 = PGC). The Snap can't power the chip, so supply 3.3 V to PIC pin 1 from a separate source, such as a USB-serial adapter's 3.3 V pin or a 3.3 V regulator. Program from VS Code with the MPLAB extension's program command, choosing the Snap as the tool.

```text
                              +----\/----+
  3.3 V supply (+) ---+-- 1 --| VDD  VSS |-- 8 --+-- 3.3 V supply (-)
  Snap pin 2 (VDD) ---+       |          |       +-- Snap pin 3 (GND)
                          2 --|          |-- 7 ----- Snap pin 4 (PGD)
                          3 --|          |-- 6 ----- Snap pin 5 (PGC)
  Snap pin 1 (MCLR) ----- 4 --| MCLR     |-- 5
                              +----------+
                       PIC12LF1572, DIP-8, top view
                       100 nF ceramic capacitor from pin 1 to pin 8
```

### Flashing the HV build

> Status: not tried on target yet. The programmer choices come from Microchip's documentation and user reports, not from this project.

The HV build (`HV_PROGRAMMING_ONLY = 1`, see [2.1](#21-the-button-and-the-two-builds)) needs a programmer that can put 8–9 V on MCLR. Only the HV build needs one; the default LVP build never does.

| Programmer                                        | Rough price | Software (free)                                                                          | Notes                                                                    |
| ------------------------------------------------- | ----------- | ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| PICkit 3 clone, for example Olimex PIC-KIT3       | $25–35      | MPLAB X IPE **v6.20** (the last version that supports the PICkit 3), or PICkitminus      | Cheapest. Known to program the PIC12F1572, the LF chip's sibling         |
| Microchip MPLAB PICkit 5 (PG164150)               | $77         | MPLAB X IPE                                                                              | Current Microchip tool. Can power the chip, and can raise VPP before VDD |
| *Can't do HV:* MPLAB Snap, MPLAB PICkit Basic     |             |                                                                                          | LVP build only                                                           |
| *Not documented here:* Raspberry Pi + Pickle      |             |                                                                                          | Needs an external 9 V switching circuit; no tested 3.3 V design          |

1. Build: `tools\build.ps1 -HvProgrammingOnly` (VS Code: **Run Task → Build HV variant**). Output: `out\cli\solrbrite-hv.hex`.
2. Wire the chip out of the light, on a breadboard or in the SOIC clamshell adapter. The programmer header uses the same pin numbers as the Snap; leave programmer pin 6 unconnected.

   ```text
                                 +----\/----+
     PICkit pin 2 (VDD) ---- 1 --| VDD  VSS |-- 8 -------- PICkit pin 3 (GND)
                             2 --|          |-- 7 -------- PICkit pin 4 (PGD)
                             3 --|          |-- 6 -------- PICkit pin 5 (PGC)
     PICkit pin 1 (VPP) ---- 4 --| MCLR     |-- 5
                                 +----------+
                          PIC12LF1572, DIP-8, top view
                          100 nF ceramic capacitor from pin 1 to pin 8
   ```
3. In the programming software, select the device **PIC12LF1572** and set the programmer to power the chip at **3.3 V**. HV entry needs VDD above 2.8 V on the PICkit 5, and bulk erase needs at least 2.7 V, so never program on the light's 2.5 V supply. Don't go above 3.6 V, the LF chip's maximum.
4. If the software offers it, choose **"Apply VPP before VDD"** (VPP-first entry). Microchip's programming spec strongly recommends it for a chip whose MCLR is disabled, which is exactly the HV build once it's on the chip.
5. Program `solrbrite-hv.hex`, then verify.

To go back to the LVP build, program `out\cli\solrbrite.hex` with the HV programmer the same way. That turns LVP back on, and the Pi and the Snap work again.

### Development hardware setup
- Remove U1 with hot air or ChipQuik. Keep it; it's a working part you can put back.
- Run 8 short wire-wrap leads from the U1 pads to a DIP-8 socket on perfboard, with 100 nF between VDD and GND at the socket. Use a **DIP** PIC12LF1572-I/P for development.
- Pins 6 and 7 double as the programming clock and data lines, and they're loaded (R9 plus the gates, and C4). Programming also needs VDD of at least 2.7 V, more than the board's 2.5 V. Either flash the chip on the breadboard, or add a 3-pin header and jumper for each of pins 1, 6 and 7 that switches the socket pin between the board and the programmer:

  ```text
    U1 pad 1 (board VCC, 2.5 V) ---- o  RUN
    socket pin 1 ------------------- o  (middle)
    programmer VDD (3.3 V) --------- o  PROG

    U1 pad 6 (board: R9, gates) ---- o  RUN
    socket pin 6 ------------------- o  (middle)
    programmer PGC ----------------- o  PROG

    U1 pad 7 (board: divider, C4) -- o  RUN
    socket pin 7 ------------------- o  (middle)
    programmer PGD ----------------- o  PROG

    Jumpers on RUN + middle:  the chip runs the board.
    Jumpers on PROG + middle: the programmer powers and reaches the chip, without the board's loads.
  ```
  Move all three jumpers together. Socket pins 4 (MCLR) and 8 (GND) stay wired to the board and also go to the programmer.
- For the final build, flash a **SOIC** PIC12LF1572-I/SN in a clamshell SOIC-8→DIP-8 adapter, solder it onto the U1 pads, and reseal with neutral-cure RTV silicone.

---

## 6. Not yet verified

- [ ] Firmware has **not been compiled** yet, in either build (XC8 wasn't installed when it was written). Expect small fixes on the first build, such as register names in the XC8 device header.
- [ ] HV build on target: RA3 weak pull-up and falling-edge wake with MCLRE = OFF (both per the datasheet), TESTING.md T3-HV.
- [ ] `tools\flash-pi.ps1` LVP check against a real XC8 HEX: CONFIG2 at byte address 0x10010, LVP = bit 13. Tested only on hand-made HEX files.
- [ ] `.mplab.json` was modeled on a Microchip PIC18 example (schema version 1.9). The `HI-TECH-LINK.code-model-rom` key (ROM range, for the HEF reservation) and the pack and toolchain versions may need adjusting: run **MPLAB: Edit Project Properties**, and confirm the HEF range is excluded in the .map file.
- [ ] The `wavedrom` blocks render in VS Code's built-in Markdown preview with `bmpenuelas.markdown-preview-wavedrom` (v0.1.0). If they don't, Markdown Preview Enhanced (`shd101wyy.markdown-preview-enhanced`, v0.8.28 or later for a security fix) renders them in its own preview.
- [ ] DFP download URL pattern (`packs.download.microchip.com/Microchip.PIC12-16F1xxx_DFP.<ver>.atpack`).
- [x] Pickle `p14` command syntax (`p14 lvp program|verify <hex>`) and BCM numbering of the config pins: confirmed from the [Pickle wiki](https://wiki.kewl.org/boards:rpi), not yet on target.
- [ ] HV programmers: a PICkit 3 clone or PICkit 5 programming the PIC12LF1572 at 3.3 V with VPP-first entry (from Microchip docs and user reports only).
- [ ] Snap fallback flow: pinout, external power, and programming from the VS Code MPLAB extension.
- [ ] No [TESTING.md](TESTING.md) step has been run on target yet.
- [ ] All [MEASURE] constants in `src/app_config.h`.
- [ ] PIR power-up and false-trigger behavior with the LEDs switching amps off B+.
- [ ] Fades are linear in duty cycle; a gamma table (TODO in `fade.c`) would look smoother.

## 7. Safety
- Disconnect the battery pack before soldering. Never bypass U3/U4: don't connect battery B− to board GND directly.
- The LEDs draw several amps straight from the cells. Keep any test wiring away from the L− side.
- Reseal the compartment and the four poked screw holes; water in the controller is the usual way these lights die.

## References
- PIC12(L)F1571/2 datasheet (DS40001723): https://www.microchip.com/en-us/product/PIC12LF1572
- Pickle (Pi GPIO PIC programmer): https://wiki.kewl.org/projects:pickle (14-bit device list: https://wiki.kewl.org/projects:pickle:14-bit)
- MPLAB XC8: https://www.microchip.com/en-us/tools-resources/develop/mplab-xc-compilers/xc8
- Microchip packs repository: https://packs.download.microchip.com/
- XB8886A (LCSC): https://www.lcsc.com/product-detail/C669690.html
- Costco AU listing (product specs): https://costco.com.au/c/Sol-R-Brite-Solar-Street-Light/p/1806387
