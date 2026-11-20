# Decisions

This file records the non-trivial decisions behind this project's design, the
alternatives that were ruled out, and why. The design itself is described in
[README.md](README.md). The rules for adding entries are in
[AGENTS.md](AGENTS.md#decisions).

Each entry gives the decision, the context, what was ruled out, and the
rationale. New entries go at the bottom. To reverse a decision, add a new
entry that names the one it replaces, and mark the old one "Superseded by Dn".

---

## D1. The button is a reset only when there's no high-voltage programmer

**Decision:** The firmware has two builds, selected by `HV_PROGRAMMING_ONLY`
in `src/app_config.h`:
- **LVP build (default, `0`):** LVP = ON. Pin 4, the original mode button, is then MCLR (the PIC's reset pin), so each press resets the chip. At boot the firmware reads the MCLR-reset flag in the PCON register and advances the mode 1 → 2 → 3 → OFF. A press only counts once the chip has run for `BUTTON_DEBOUNCE_MS` without another reset.
- **HV build (`1`):** LVP = OFF and MCLRE = OFF. Pin 4 is RA3, a normal input with its internal pull-up and interrupt-on-change. The firmware debounces it on the 10 ms tick and advances the mode on release, without a reset. A press wakes the chip from Sleep.

**Context:** The Raspberry Pi + Pickle and the MPLAB Snap only do low-voltage
programming (LVP): they put the chip into programming mode at normal logic
levels, without the 8–9 V on MCLR that high-voltage (HV) programming needs.
While LVP = ON, the PIC ignores its MCLRE setting and pin 4 is always MCLR.
The board wires the button to pin 4. The chip refuses to clear LVP during
low-voltage programming, and once LVP = OFF only HV programming can enter
programming mode (datasheet DS40001723, CONFIG2 note; programming spec
DS40001713). So only an HV programmer can write the HV build, or replace it.

**Ruled out:**
- **One build with LVP = OFF.** It locks out the Pi and the Snap, which are the main programmers (D2).
- **One build with LVP = ON only.** It wastes the pin for anyone who owns an HV programmer: each press resets the chip, which blanks the LEDs while the button is held, cancels a running motion hold and re-reads day/night from a single sample.
- **Moving the button to another pin.** It needs board rework, and pins 2, 3, 5, 6 and 7 are all in use.

**Rationale:** The default build works with the cheap programmers, and the
board stays unmodified around the button. The HV build uses the button pin
fully for those who can program it. Both builds share all other code, and
the only differences are behind `#if HV_PROGRAMMING_ONLY`.

**Consequences:**
- LVP build: nothing runs while the button is held (the chip is in reset), so a press can't be timed: no long-press function. A bouncy press produces several resets in a row; `BUTTON_DEBOUNCE_MS` filters them.
- HV build: the Pi or the Snap would write it with LVP still on, leaving pin 4 as MCLR and a button that does nothing. So `tools\flash-pi.ps1` refuses a HEX with LVP off, and the HV output has its own name (`solrbrite-hv.hex`).
- HV build: Microchip's programming spec strongly recommends VPP-first entry (VPP raised before VDD) for a chip with MCLR disabled, so the docs tell HV programmer users to pick it.
- Both builds advance the mode on release, so the button feels the same.

## D2. Pi + Pickle first, MPLAB Snap as fallback, an HV programmer only for the HV build

**Decision:**
- **LVP build (the default):** docs and scripts use a Raspberry Pi's GPIO pins with Pickle as the main programmer. The MPLAB Snap (PG164100, about $35–40) is the documented fallback for people without a Pi.
- **HV build (optional, D1):** a high-voltage programmer. The docs name a PICkit 3 clone (about $25–35) with MPLAB X IPE v6.20 or PICkitminus, and the PICkit 5 (PG164150, about $77) with MPLAB X IPE.

**Context:** AGENTS.md sets the tool order: free and open source first, then
free closed source, then cheap hardware. Most hobbyists already own a Pi. Only
an HV programmer can write the HV build (D1). The Snap and the new MPLAB
PICkit Basic can't do HV programming. Pickle can drive HV programming, but the
Pi can't make 8–9 V itself: it needs an external switching circuit, the
documented ones are for 5 V chips, and Pickle only raises VDD before VPP.

**Ruled out:**
- **PICkit 4/5 or MPLAB ICD as the main path.** They cost much more than the Snap, and their main extra, HV programming, is only needed for the optional HV build.
- **Pi + Pickle with a home-made 9 V switch for the HV build.** No tested design for a 3.3 V chip, and no VPP-first entry, which the programming spec strongly recommends for the HV build. Worth revisiting if someone builds and tests one: it would make the HV build free for Pi owners.
- **PICkit 2 clones.** The last official device file doesn't include the PIC12LF1572. A community device file (PICkitminus) lists it, but its HV behavior on this chip is unconfirmed.
- **PICkit 4.** No longer sold new since September 2023; the PICkit 5 replaces it.

**Rationale:** The default path costs nothing extra for Pi owners, and the Snap
is the cheapest Microchip programmer for everyone else. The HV build is
optional, so no one has to buy an HV programmer. The PICkit 3 clone is the
cheapest HV tool known to program this chip family. The PICkit 5 is the
supported, current one.

**Consequences:** MPLAB X IPE dropped the PICkit 3 after v6.20, so PICkit 3
owners use that archived version or PICkitminus (free of charge, closed
source).

## D3. MPLAB XC8 in free mode is the compiler

**Decision:** The firmware is C99 built with MPLAB XC8 in free mode. No
XC8 PRO features. The command-line build (`tools\build.ps1`) works without
VS Code or MPLAB X IDE.

**Context:** AGENTS.md prefers open-source tools, and XC8 is closed source.

**Ruled out:**
- **Open-source C compilers** (such as SDCC's PIC14 port). None handles the PIC12LF1572 reliably.
- **XC8 PRO** (better optimization). It needs a paid license.

**Rationale:** XC8 free mode is the only reliable free C compiler for this
chip, so it's the accepted exception to the open-source-first rule.

## D4. The mode is stored in high-endurance flash (HEF)

**Decision:** The selected mode is saved at address 0x780 as one byte plus an
inverted copy. The build keeps program code out of 0x780–0x7FF
(`-mrom=default,-780-7ff` on the command line, and the matching setting in
`.vscode/pic-hack-solrbrite.mplab.json`). If the two copies don't match (a
blank chip, or an interrupted write), the firmware uses `DEFAULT_MODE`.

**Context:** The PIC12LF1572 has no data EEPROM. Instead, the last 128 words of
its program flash, 0x780–0x7FF, are high-endurance flash, rated for about
100,000 erase/write cycles against about 10,000 for the rest of the flash.

**Ruled out:**
- **Keeping the mode in RAM only.** It would be lost on every power-on reset, for example when the battery is disconnected or the protection ICs cut off a flat pack.

**Rationale:** HEF is the chip's intended place for settings, and a mode
change (one button press) is rare enough that wear isn't a concern. The
inverted copy is a cheap way to tell a valid record from a blank or corrupted
one.

## D5. No interrupt handler

**Decision:** The firmware has no interrupt service routine (ISR, a function
the hardware calls when an event happens). The main loop waits on a 10 ms
Timer2 tick. Sleep ends on the watchdog timer (every `WDT_PERIOD_S`) or on a
PIR rising edge through interrupt-on-change, with interrupts globally disabled
(GIE = 0).

**Context:** Everything the firmware times is slow: fades, debounce, motion
hold and sensor settling all work at 10 ms resolution.

**Ruled out:**
- **An interrupt-driven tick and PIR input.** It adds shared state between the ISR and the main loop, and the race conditions that come with it, without making anything this firmware does more accurate.

**Rationale:** One loop keeps all state in one place. On this PIC an enabled
interrupt-on-change source wakes the chip from Sleep even with GIE = 0, so
waking needs no ISR either. With GIE never set, the timed HEF write sequence
(D4) can't be interrupted. Adding an ISR needs a clear reason, recorded here
as a new entry.
