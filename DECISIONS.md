# Decisions

This file records the non-trivial decisions behind this project's design, the
alternatives that were ruled out, and why. The design itself is described in
[README.md](README.md). The rules for adding entries are in
[AGENTS.md](AGENTS.md#decisions).

Each entry gives the decision, the context, what was ruled out, and the
rationale. New entries go at the bottom. To reverse a decision, add a new
entry that names the one it replaces, and mark the old one "Superseded by Dn".

---

## D1. Low-voltage programming stays on, so the button is the reset pin

**Decision:** The configuration bits always set LVP = ON. That makes pin 4,
the original mode button, MCLR (the PIC's reset pin), so each press resets the
chip. At boot the firmware reads the MCLR-reset flag in the PCON register and
advances the mode 1 → 2 → 3 → OFF. A press only counts once the chip has run
for `BUTTON_DEBOUNCE_MS` without another reset.

**Context:** The supported programmers (Raspberry Pi + Pickle, MPLAB Snap)
only do low-voltage programming (LVP): they put the chip into programming mode
without the roughly 9 V on MCLR that high-voltage programming needs. With
LVP = ON, the PIC ignores its MCLRE setting and pin 4 is always MCLR, with a
weak pull-up. The board wires the button to pin 4.

**Ruled out:**
- **LVP = OFF, with the button read as a normal input (RA3).** This frees pin 4, but only a high-voltage programmer (PICkit 4/5) can reprogram the chip afterwards. Neither the Pi nor the Snap can recover it, and the cost rules in AGENTS.md exclude the PICkit.
- **Moving the button to another pin.** It needs board rework, and pins 2, 3, 5, 6 and 7 are all in use.

**Rationale:** Every user can reflash the chip with the cheap programmers, and
the board stays unmodified around the button.

**Consequences:** Nothing runs while the button is held down (the chip is in
reset), so the firmware can't measure how long a press lasts: there is no
long-press function. A bouncy press produces several resets in a row;
`BUTTON_DEBOUNCE_MS` filters them.

## D2. Raspberry Pi + Pickle is the main programmer, the MPLAB Snap the fallback

**Decision:** Docs and scripts use a Raspberry Pi's GPIO pins with Pickle as
the main programmer. The MPLAB Snap (PG164100, about $35–40) is the documented
fallback for people without a Pi.

**Context:** AGENTS.md sets the tool order: free and open source first, then
free closed source, then cheap hardware. Most hobbyists already own a Pi.

**Ruled out:**
- **PICkit 4/5 or MPLAB ICD as the main path.** They work, but cost much more than the Snap, and their main extra, high-voltage programming, isn't needed with LVP on (D1).

**Rationale:** Pickle is open source and costs nothing extra for anyone who
owns a Pi. The Snap is the cheapest Microchip programmer for everyone else.

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
