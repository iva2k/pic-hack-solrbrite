# AGENTS.md

Guidance for anyone, human or AI agent, working on this repository. Read it
before changing code, docs or tooling.

## What this project is

Open-source (MIT) replacement firmware for the controller of the Costco
#1806387 Fresh Source / Sol-R-Brite 5000 lm motion solar light. The original
one-time-programmable MCU (U1) is swapped for a PIC12LF1572 so the always-on
50 lm night glow can be turned off. Hardware findings, pinout and behavior are
in [README.md](README.md); the on-target test procedure is in
[TESTING.md](TESTING.md).

## Who the docs are for

A hobbyist with basic DIY electronics skills: can solder through-hole and
simple SMD, use a multimeter, follow a wiring table, and run commands someone
else wrote. They may own a cheap scope. They do not know PIC internals,
MPLAB, or embedded C.

When writing docs:
- Explain a term the first time it appears (for example "MCLR, the PIC's reset pin").
- Give steps in order, with the exact command to run and what a good result looks like.
- Name parts with an orderable part number and a rough price.
- Put safety warnings (Li-ion cells, several amps on the LED side, hot air) right where the risky step is, not only in a safety section.
- Prefer a wiring table or a short list over prose.
- Keep register-level detail in code comments or a clearly marked "details" section, so the main path stays readable.

## Tools and cost

Pick tools in this order:
1. Free and open source.
2. Free of charge, closed source, when no workable open-source option exists.
3. Cheap commercial hardware, only when nothing free does the job.

What that means today:
- **Compiler:** MPLAB XC8 in free mode. It is closed source; it is the accepted exception, since no open-source C compiler handles the PIC12LF1572 reliably. Never use XC8 PRO features or anything that needs a paid license.
- **Editor:** VS Code with the free MPLAB extension pack. The command-line build (`tools/build.ps1`) must always work without the extension or MPLAB X IDE.
- **Programmer, primary:** Raspberry Pi (3B+/4B) GPIO with Pickle, open source, low-voltage programming. Most hobbyists already own a Pi.
- **Programmer, fallback:** MPLAB Snap (PG164100), the cheapest Microchip programmer. Document it as the alternative, not the main path.
- Don't require PICkit 4/5, MPLAB ICD or other expensive tools. Don't add paid services, cloud build steps or accounts.
- Host scripts are PowerShell for Windows (`tools/*.ps1`); Pi-side scripts are POSIX `sh` (`tools/pi/*.sh`). Scripts install only free tools, and say what they install before doing it.

## Code, docs and tests go in lockstep

Every change updates code, docs and tests together, in the same commit. A
change that touches only one of them is incomplete.

- **Behavior change** (firmware logic, timing, a constant's meaning): update `src/`, the behavior and calibration sections of README.md, and the matching test in TESTING.md.
- **New or renamed constant** in `src/app_config.h`: document it in the README calibration table if it needs measuring, and mark it `[MEASURE]` in the code until it has been measured.
- **Wiring or hardware change:** update the README pinout and wiring tables and the TESTING.md setup steps.
- **Tooling change** (scripts, VS Code tasks, versions): update the README toolchain section and the commands in TESTING.md.
- **Known gaps:** keep README "Not yet verified" accurate. Add an item when you write something you couldn't check; tick it only after it has been checked.

### What "tested" means here

The test burden is deliberately small: the proof is running on the target.
- **Build check (every change):** `tools/build.ps1` finishes with no errors and no new warnings. If the compiler isn't available, say so; never claim a build you didn't run.
- **On-target check:** the steps in TESTING.md, run on a real chip in the light or on a breadboard. Add or update the step for any behavior you change. No unit-test framework or simulator is required; don't add one without asking.
- **Reporting:** a commit message or PR says which TESTING.md steps were run on target, or plainly "not run on target".

## Firmware rules that protect the hardware

- **Never set `LVP = OFF`** in `src/config_bits.c`. The Pi and the Snap only do low-voltage programming; a chip programmed with LVP off can't be reprogrammed by either.
- Keep program memory out of the HEF settings area, 0x780–0x7FF (`-mrom=default,-780-7ff` in the build and the matching `.mplab.json` setting).
- RA1 (pin 6) drives the MOSFET gates. It must be low whenever the firmware isn't deliberately lighting the LEDs, including at reset and during sleep.
- All tunable numbers live in `src/app_config.h`. Don't scatter magic numbers in other files.
- C99, XC8. No interrupt handler (the main loop polls the Timer2 tick); keep it that way unless there's a clear reason, and document the reason.

## Repository layout

```
src/           firmware (app_config.h holds every tunable constant)
tools/         setup, build and flash scripts (Windows PowerShell)
tools/pi/      scripts that run on the Raspberry Pi
.vscode/       VS Code tasks, settings, MPLAB project file
refs/          local-only vendor PDFs (git-ignored, see Housekeeping)
README.md      hardware notes, behavior, calibration, build and flash guide
TESTING.md     on-target test procedure
AGENTS.md      this file
CLAUDE.md      imports AGENTS.md for Claude Code; keep rules here, not there
```

## Housekeeping

- Line endings: `.sh` LF, `.ps1` CRLF (enforced by `.gitattributes`).
- Build output goes to `out/`, downloads to `tools/downloads/`, device packs to `tools/packs/`; all are git-ignored.
- Vendor datasheets and app notes are copyrighted: link to the vendor page, don't commit the PDF. Local copies can live in `refs/` (`refs/*.pdf` is git-ignored).
- Ask before adding a dependency, a new tool or a new hardware part to the bill of materials.
