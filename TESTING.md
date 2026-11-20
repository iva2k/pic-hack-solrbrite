# Testing

This project has no unit tests. The proof is the firmware running on a real
PIC12LF1572, either on a breadboard or in the light. This file is the test
procedure. Run the steps that cover what you changed, and say in the commit
message which steps you ran (for example "T0, T3, T5 on breadboard").

When a change adds or alters behavior, update the matching step here in the
same commit (see [AGENTS.md](AGENTS.md)).

The timings below are the defaults in `src/app_config.h`. If you change a
constant, the expected result changes with it.

## What you need

- A programmed PIC12LF1572 and the Raspberry Pi programming setup from the README (or an MPLAB Snap).
- A multimeter. A scope helps for T9 but isn't required for the rest.
- For breadboard tests (setup B below): breadboard, jumper wires, one LED plus a 1 kΩ resistor, two push buttons, one toggle switch or a jumper, two 10 kΩ potentiometers, a 100 kΩ resistor, a 100 nF capacitor.
- Optional, for T8: an adjustable bench power supply (about 3–4.5 V, current limit set to 1 A or less).

## Test setups

### A. In the light (the real thing)

The PIC is soldered or socketed in place of U1, and the light runs from its
own batteries. This is the final proof. Watch out: the LEDs pull several amps
straight from the 18650 cells, so keep scope and meter leads away from the
L− wire and the MOSFETs.

### B. On a breadboard (quick checks, no risk to the light)

Power the PIC from the Pi's 3.3 V pin (physical pin 1) and GND (pin 6), with
the 100 nF capacitor across PIC pins 1 and 8. Then wire:

- **Pin 2 (PIR):** 100 kΩ to GND, and a push button from pin 2 to 3.3 V. Pressing the button acts as "motion".
- **Pin 3 (light):** potentiometer wiper. Its ends go to 3.3 V and GND. Turn it up for "day", down for "night".
- **Pin 4 (mode button):** push button to GND. No resistor needed; the chip has its own pull-up on this pin.
- **Pin 5 (night-light switch):** toggle switch or jumper to GND.
- **Pin 6 (LED):** 1 kΩ resistor to the LED's long leg, short leg to GND.
- **Pin 7 (battery):** second potentiometer wiper, ends to 3.3 V and GND. Set it to about 1.6 V for a healthy battery.

Note on voltages: the firmware assumes the light's 2.5 V supply. On a 3.3 V
breadboard every voltage threshold trips at 3.3 / 2.5 = 1.32 times the value
in `app_config.h`. For example, `LIGHT_NIGHT_MV` 300 becomes about 0.40 V at
pin 3, and the 3300 mV battery cutoff happens at about 1.49 V on pin 7.

Pins 6 and 7 are also the programming pins. Disconnect the LED and the pin 7
potentiometer while flashing, or program the chip before wiring them.

## Steps

### T0. Build (every change)
Run `tools\build.ps1` (or Ctrl+Shift+B in VS Code).
- Pass: no errors and no new warnings; `out\cli\solrbrite.hex` is created.
- Open `out\cli\solrbrite.map` and check nothing is placed at 0x780–0x7FF (the settings area).

### T1. Program and verify
On the Pi, with the chip wired for programming: `p14 lvp id`, then flash with `tools\flash-pi.ps1` from Windows.
- Pass: `id` reports a PIC12LF1572, and the verify step reports no errors.

### T2. Power-up
Set "night" (pin 3 low), switch open. Power on.
- Pass: the LED stays off. Pressing "motion" in the first 30 s (`PIR_WARMUP_S`) does nothing; after 30 s it does (see T5).

### T3. Mode button and preview
Press the mode button once, wait, and repeat.
- Pass: each press lights the LED at the next level for 3 s (`PREVIEW_S`), then it fades off. The order is 1 → 2 → 3 → OFF → 1. At OFF the LED stays dark.
- Pass: a single press advances exactly one step, even with a bouncy button.
- Pass: power off, power on. The mode is the same as before power-off (it's saved in the chip).

### T4. Day and night detection
With "motion" held off, move the light pot between day and night.
- Pass: the change is only accepted after about 60 s at the new level (30 samples, 2 s apart). A brief flick of the pot does nothing.
- Pass: in "day", motion never lights the LED.

### T5. Motion at night
Mode 1, 2 or 3, "night" confirmed (T4), warm-up finished (T2). Press "motion" briefly.
- Pass: the LED fades up over about 0.8 s (`FADE_ON_MS`) to the mode's level.
- Pass: it stays on for 30 s (`MOTION_HOLD_S`) after the last motion; pressing "motion" again restarts the 30 s.
- Pass: it then fades down over about 1.5 s (`FADE_OFF_MS`). Motion in the next 1.5 s after the fade ends (`PIR_LOCKOUT_MS`) is ignored.

### T6. Night-light switch
At night, mode 1–3:
- Pass: switch closed, the LED glows at the dim night-light level. About once a minute (`NIGHTLIGHT_SENSE_S`) it blinks off briefly while the light level is checked.
- Pass: motion still brings it up to the mode level, and afterwards it fades back to the glow, not to off.
- Pass: switch open, the LED is off between motions.
- Pass: in "day", the LED is off whatever the switch says.

### T7. Mode OFF
Select OFF with the mode button (T3). Close the night-light switch, set "night", press "motion".
- Pass: the LED never lights.

### T8. Low battery (optional, needs a bench supply or the pin 7 pot)
In the light: power B+ from the bench supply instead of the cells. On the breadboard: use the pin 7 pot.
- Pass: below 3.3 V battery (`BATT_CUTOFF_MV`) the LED won't light on motion or as a night light.
- Pass: it works again only after the battery rises above 3.55 V (`BATT_RESUME_MV`), not as soon as it creeps back over 3.3 V.

### T9. PWM timing (scope)
Probe pin 6 against GND.
- Pass: the period matches `PWM_PERIOD_US` (1000 µs by default).
- Pass: the high time at each level matches `LEVEL0_HIGH_US` to `LEVEL3_HIGH_US`. After calibration, these should match the stock controller's readings.

### T10. Sleep current (optional)
In the light, daytime, LEDs off. Measure the current in one battery lead (meter in series, µA or mA range).
- Pass: no higher than the stock controller's daytime current, measured the same way before the swap.

### T11. Field test
Install the light outdoors for at least one full night.
- Pass: it turns on at dusk, reacts to motion at the chosen level, stays dark between motions (switch open), and stops reacting after dawn. The battery is recharged by the next evening.
