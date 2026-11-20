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

- **PIC12LF1572 chips** (Microchip): a DIP-8 one (PIC12LF1572-I/P) for the breadboard and a SOIC-8 one (PIC12LF1572-I/SN) for the final build. Roughly $1 each.
- **Programmer:** a Raspberry Pi 3B+ or 4B you already own, plus jumper wires. If you don't have a Pi, the fallback is a Microchip **MPLAB Snap** (PG164100), roughly $35–40.
- **SOIC-8 to DIP-8 clamshell adapter**, to flash the SMD chip before soldering it. Roughly $5–15.
- **Tools:** soldering iron, hot air or ChipQuik to remove the old chip, multimeter. A scope is useful for calibration but not required to get going.
- **Software (free):** VS Code, the MPLAB extension pack, the MPLAB XC8 compiler in free mode, and Pickle on the Pi. `tools\setup.ps1` installs the Windows side.

### Other docs
- [TESTING.md](TESTING.md): how to check the firmware on a breadboard and in the light.
- [AGENTS.md](AGENTS.md): project rules for contributors (tools, doc style, code/docs/tests in lockstep).

---

## 1. Original hardware (as found)

### 1.1 Product
| Item | Value |
|---|---|
| Retail | Costco item 1806387 (US), also sold as Sol-R-Brite (Costco AU 1806387, Walmart) |
| Model | Fresh Source FSI 80-8572-M |
| UPC | 850049867979 |
| Rated output | 1000 / 2000 / 5000 lm on motion, 50 lm steady glow when idle at night; 120° PIR |
| Stock behavior | Button: 3 modes + off. In every mode: 50 lm glow dusk to dawn, full level on motion |

### 1.2 Power path
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
- **Pin layout:** VDD on pin 1, GND on pin 8, input-only pin 4. This is the PIC12F-style layout that many Chinese one-time-programmable MCUs copy (Padauk PMS15x, Sinomcu MC30P60xx, FMD FT60F01x, ...). The firmware can't be read out.

| Pin | Net | Components |
|---|---|---|
| 1 | VCC 2.5 V | U2 output |
| 2 | PIR.a | PIR digital output |
| 3 | Light sense | phototransistor to VCC; **R1 330 kΩ** ‖ C3 to GND |
| 4 | Button | push button to GND |
| 5 | not connected | |
| 6 | LED drive | **R9 620 Ω** to Q1–Q5 gates; **R8 100 kΩ** from gates to GND |
| 7 | Battery sense | **R3 1.2 MΩ** from B+; **R2 620 kΩ** ‖ C4 to GND |
| 8 | GND | |

### 1.5 Resistor codes
| Ref | Code | Value | Role |
|---|---|---|---|
| R1 | 3303 | 330 kΩ | Light sensor load (pin 3 to GND) |
| R2 | 6203 | 620 kΩ | Battery divider, bottom leg |
| R3 | 1204 | 1.2 MΩ | Battery divider, top leg |
| R8 | 104 | 100 kΩ | Gate pull-down (LEDs stay off while U1 is in reset or missing) |
| R9 | 6200 | 620 Ω | Gate series resistor |

### 1.6 Useful numbers
- **Battery divider:** 620k / 1.82M = **0.341**. At 4.2 V the pin sees 1.43 V; at 3.0 V, 1.02 V. Source impedance is about 410 kΩ, and C4 holds the voltage up during ADC sampling. The divider draws about 2.3 µA.
- **Light sense:** pin 3 ≈ photocurrent × 330 kΩ. That's about 0 V in the dark, 0.33 V at 1 µA, and near VCC in daylight. Leakage in the dark rises with temperature (tens of mV on a hot day).
- **Gate drive:** 620 Ω into five gates (about 5 nF total, estimated) gives a time constant of about 3 µs. Switching is slow, so keep the PWM frequency low, close to the stock value.
- **Night glow cost (estimated):** about 0.4 W for 12 h is about 5 Wh per night, more than 10% of the pack's roughly 40 Wh. That's the main reason for this project.

---

## 2. Replacement MCU: PIC12LF1572

Datasheet: PIC12(L)F1571/2, DS40001723, free from the [Microchip product page](https://www.microchip.com/en-us/product/PIC12LF1572) (Documentation tab). It is not included in this repo; if you keep a local copy, put it in `refs/` (git-ignored). The LF version runs on 1.8–3.6 V, so 2.5 V VCC is fine. It has 16-bit PWMs and high-endurance flash (HEF) for settings.

| Pin | PIC function | Use in this firmware |
|---|---|---|
| 1 | VDD | 2.5 V |
| 2 | RA5 (interrupt-on-change) | PIR input; a rising edge wakes the chip from Sleep |
| 3 | RA4 / AN3 | Light level (ADC) |
| 4 | RA3 / MCLR | Button = **MCLR reset** (see below) |
| 5 | RA2 | **Night-light switch** to GND, weak pull-up enabled only while reading |
| 6 | RA1 / PWM1 | LED PWM (APFCON.P1SEL = 0) |
| 7 | RA0 / AN0 | Battery voltage (ADC) |
| 8 | VSS | GND |

**Why the button is a reset.** The supported programmers (Raspberry Pi + Pickle as the main option, MPLAB Snap as the fallback) can only do **low-voltage programming (LVP)**. With LVP=ON, pin 4 is always MCLR with its weak pull-up, and MCLRE is ignored. Each press resets the chip. At boot the firmware sees the MCLR-reset flag (PCON) and advances the mode. **Never program LVP=OFF** unless you have a high-voltage programmer (PICkit 4/5); the Pi and the Snap can't recover the chip after that.

---

## 3. Firmware behavior

| Situation | LEDs |
|---|---|
| Day (light above `LIGHT_DAY_MV`, confirmed) | Off. The MCU sleeps and wakes every ~2 s on the watchdog to sample light, battery and the switch |
| Night, idle, pin-5 switch **open** | Off; sleeping, woken by PIR edges |
| Night, idle, pin-5 switch **closed** | Level 0 (~50 lm night light). Briefly blanked every `NIGHTLIGHT_SENSE_S` to check for dawn |
| Night, motion | Fade up to the mode level (1/2/3) and hold for `MOTION_HOLD_S` after the last motion, then fade back to idle |
| Button press (MCLR) | Mode 1 → 2 → 3 → OFF → 1, saved in HEF. The new mode's brightness shows for `PREVIEW_S` (N = 3 s), then normal operation. OFF = no light at all |
| Battery below `BATT_CUTOFF_MV` | All LEDs disabled until the battery recovers above `BATT_RESUME_MV` |
| After power-on | PIR ignored for `PIR_WARMUP_S` |
| After the light goes off | PIR ignored for the fade-off time plus `PIR_LOCKOUT_MS`, so the supply transient doesn't retrigger it |

Light and battery are only sampled when the LEDs have been fully off for `LIGHT_SETTLE_MS`. That keeps the sensor from seeing the light's own LEDs (it looks down at the lit ground) and avoids battery sag under load.

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

| Constant(s) | What to measure | Where |
|---|---|---|
| `PWM_PERIOD_US` | PWM period | U1 pin 6 |
| `LEVEL0..3_HIGH_US` | High time for the glow and for each mode on motion | U1 pin 6 |
| `FADE_ON_MS`, `FADE_OFF_MS` | Stock ramp times, if any (or choose your own) | U1 pin 6 |
| `MOTION_HOLD_S` | Time on after the last motion | U1 pin 6 |
| `LIGHT_NIGHT_MV`, `LIGHT_DAY_MV` | Pin 3 voltage at the moment the stock unit switches at dusk and at dawn | U1 pin 3 |
| `LIGHT_SETTLE_MS` | Pin 3 recovery time after a light shining on it is removed (330k × C3) | U1 pin 3 |
| PIR behavior | Idle level, pulse width on motion, behavior during the first minute after power-up | U1 pin 2 |
| `BATT_*` | Check pin 7 ≈ 0.341 × B+ | U1 pin 7 |
| Sleep current | Board current in daytime, as the baseline to beat | battery lead |

Also worth checking: whether the stock firmware samples pin 3 during PWM off-time while the glow is on.

---

## 5. Toolchain setup (Windows)

Run once (VS Code: **Terminal → Run Task → Setup: install toolchain**):
```powershell
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
```
It installs:
1. **VS Code extensions:** `Microchip.mplab-extension-pack` and the PowerShell extension (also listed in `.vscode/extensions.json`).
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
2. Wiring, PIC powered from the Pi at 3.3 V. Program at 3.3 V: full-chip erase on many PICs needs more than the board's 2.5 V.

   | Pi physical pin | Signal | PIC12LF1572 DIP pin |
   |---|---|---|
   | 1 | 3V3 | 1 VDD (+100 nF to GND) |
   | 6 | GND | 8 VSS |
   | 7 (GPIO4) | VPP/MCLR | 4 |
   | 22 (GPIO25) | PGC | 6 |
   | 18 (GPIO24) | PGD | 7 |
3. From Windows: **Run Task → Flash via Raspberry Pi**, or
   ```powershell
   tools\flash-pi.ps1 -PiHost pi@raspberrypi.local
   ```
   (needs SSH key login). It copies the HEX to the Pi and runs `p14 lvp program`, then `p14 lvp verify`.

### Flashing with an MPLAB Snap (fallback)
If you don't have a Pi, an MPLAB Snap (PG164100) works over USB from Windows. Use the same five connections as the Pi table (Snap pin 1 = MCLR, 2 = VDD, 3 = GND, 4 = PGD, 5 = PGC). The Snap can't power the chip, so supply 3.3 V to PIC pin 1 from a separate source, such as a USB-serial adapter's 3.3 V pin or a 3.3 V regulator. Program from VS Code with the MPLAB extension's program command, choosing the Snap as the tool.

### Development hardware setup
- Remove U1 with hot air or ChipQuik. Keep it; it's a working part you can put back.
- Run 8 short wire-wrap leads from the U1 pads to a DIP-8 socket on perfboard, with 100 nF between VDD and GND at the socket. Use a **DIP** PIC12LF1572-I/P for development.
- Pins 6 and 7 double as the programming clock and data lines, and they're loaded (R9 plus the gates, and C4). Either flash the chip on the breadboard, or add 2-position jumpers that switch pins 6 and 7 between the board and a programming header.
- For the final build, flash a **SOIC** PIC12LF1572-I/SN in a clamshell SOIC-8→DIP-8 adapter, solder it onto the U1 pads, and reseal with neutral-cure RTV silicone.

---

## 6. Not yet verified

- [ ] Firmware has **not been compiled** yet (XC8 wasn't installed when it was written). Expect small fixes on the first build, such as register names in the XC8 device header.
- [ ] `.mplab.json` was modeled on a Microchip PIC18 example (schema version 1.9). The `HI-TECH-LINK.code-model-rom` key (ROM range, for the HEF reservation) and the pack and toolchain versions may need adjusting: run **MPLAB: Edit Project Properties**, and confirm the HEF range is excluded in the .map file.
- [ ] DFP download URL pattern (`packs.download.microchip.com/Microchip.PIC12-16F1xxx_DFP.<ver>.atpack`).
- [ ] Pickle `p14` command syntax (`p14 lvp program|verify <hex>`) and whether the config pin numbers are BCM.
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
