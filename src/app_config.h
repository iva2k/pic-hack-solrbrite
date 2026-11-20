/*
 * app_config.h - All tunable constants for the Sol-R-Brite / Fresh Source
 *                replacement firmware (PIC12LF1572).
 *
 * Values marked [MEASURE] are placeholders until they are measured on the
 * original controller with a scope (see README.md, "Calibration").
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* System timing                                                             */
/* ------------------------------------------------------------------------- */

/* CPU clock: HFINTOSC 4 MHz (OSCCON.IRCF = 1101). hal.c assumes this value
 * for the Timer2 tick setup; change both together. */
#define FOSC_HZ                 4000000UL
#define _XTAL_FREQ              FOSC_HZ     /* for __delay_us()/__delay_ms() */

/* Main loop tick while awake (Timer2). hal.c is set up for 10 ms. */
#define TICK_MS                 10u
#define TICKS_PER_S             (1000u / TICK_MS)
#define MS_TO_TICKS(ms)         ((uint16_t)(((ms) + TICK_MS - 1u) / TICK_MS))

/* Watchdog wake-up interval while sleeping (WDTCON.WDTPS code + its period).
 * 0b01011 = 1:65536 = 2 s nominal (LFINTOSC based, +/- tens of %). */
#define WDT_PS_CODE             0x0Bu
#define WDT_PERIOD_S            2u

/* ------------------------------------------------------------------------- */
/* PWM output (pin 6, RA1/PWM1 -> R9 620R -> Q1..Q5 gates, R8 100k pull-down) */
/* ------------------------------------------------------------------------- */

/* PWM1 clock = FOSC >> PWM_PRESCALE_LOG2 (PWM1CLKCON.PS, 0..7). */
#define PWM_PRESCALE_LOG2       0u
#define PWM_CLK_HZ              (FOSC_HZ >> PWM_PRESCALE_LOG2)

/* Convert microseconds to PWM counts (no casts: also used in #if). */
#define PWM_US_TO_COUNTS(us)    ((us) * (PWM_CLK_HZ / 1000UL) / 1000UL)

/* PWM period [MEASURE]: R9 + 5 gates switch slowly (~us edges), keep the
 * frequency similar to the original controller (likely 0.3..2 kHz). */
#define PWM_PERIOD_US           1000UL

/* High time per brightness level [MEASURE].
 * Level 0 = night light (~50 lm), 1 = 1000 lm, 2 = 2000 lm, 3 = 5000 lm.
 * A value >= PWM_PERIOD_US means 100 % (output constantly on). */
#define LEVEL0_HIGH_US          10UL        /*   50 lm  night light */
#define LEVEL1_HIGH_US          200UL       /* 1000 lm  mode 1      */
#define LEVEL2_HIGH_US          400UL       /* 2000 lm  mode 2      */
#define LEVEL3_HIGH_US          1000UL      /* 5000 lm  mode 3      */

/* Fade (gradual on/off) durations. Every transition completes in this time
 * regardless of the start and end brightness. 0 = switch instantly. */
#define FADE_ON_MS              800u        /* toward a brighter level */
#define FADE_OFF_MS             1500u       /* toward a dimmer level / off */

/* ------------------------------------------------------------------------- */
/* Button (pin 4, RA3/MCLR to GND)                                           */
/* ------------------------------------------------------------------------- */
/* LVP stays enabled (Pickle / MPLAB Snap program in LVP mode only), so pin 4
 * is always MCLR with its weak pull-up. A press resets the MCU; firmware
 * detects the MCLR reset and advances the mode: 1 -> 2 -> 3 -> OFF -> 1.   */

/* After a mode change the LEDs show that mode's brightness for N seconds. */
#define PREVIEW_S               3u

/* Contact bounce on press/release produces several MCLR resets. The mode is
 * only advanced after the MCU has run this long without another reset. */
#define BUTTON_DEBOUNCE_MS      60u

/* Mode used when nothing valid is stored in HEF (first boot). 0=OFF, 1..3. */
#define DEFAULT_MODE            3u

/* ------------------------------------------------------------------------- */
/* Night-light switch (pin 5, RA2, switch to GND, internal weak pull-up)     */
/* ------------------------------------------------------------------------- */
/* Closed (0) = night light (level 0, ~50 lm) on while idle at night.
 * Open   (1) = night light off -> LEDs only on motion.
 * The pull-up is enabled only for the moment of reading to save current.   */

/* ------------------------------------------------------------------------- */
/* Motion sensor (pin 2, RA5, digital PIR output, active high)               */
/* ------------------------------------------------------------------------- */
#define MOTION_HOLD_S           30u     /* LEDs stay on this long after last motion */
#define PIR_WARMUP_S            30u     /* ignore PIR after power-on reset          */
#define PIR_LOCKOUT_MS          1500u   /* ignore PIR after the LEDs finish fading
                                           off (supply transient / thermal)        */

/* ------------------------------------------------------------------------- */
/* Supply and analog inputs                                                  */
/* ------------------------------------------------------------------------- */
#define VDD_MV                  2500UL  /* U2 regulator output = ADC reference */

/* ADC acquisition time before conversion. Both sources are high impedance
 * but have filter caps (C3, C4), so a short time is enough. */
#define ADC_ACQ_US              20u

/* Light sensor (pin 3, RA4/AN3): phototransistor from VCC, R1 330k || C3 to
 * GND, looking out through the PIR window. Pin voltage rises with light.
 * Thresholds in mV at the pin [MEASURE at real dusk/dawn]. */
#define LIGHT_NIGHT_MV          300UL   /* below -> counts toward NIGHT */
#define LIGHT_DAY_MV            600UL   /* above -> counts toward DAY   */
#define LIGHT_CONFIRM_SAMPLES   30u     /* consecutive samples (~2 s each) */

/* Time the LEDs must be fully off before a light sample is trusted
 * (330k * C3 settling + phototransistor) [MEASURE]. */
#define LIGHT_SETTLE_MS         200u

/* With the night light on, the LEDs are briefly blanked this often to take a
 * clean light sample (to detect dawn). */
#define NIGHTLIGHT_SENSE_S      60u

/* How often light/battery/switch are checked while awake. */
#define SENSE_PERIOD_S          2u

/* Battery sense (pin 7, RA0/AN0): B+ -> R3 1.2M -> pin -> R2 620k || C4 -> GND */
#define BATT_R_TOP_K            1200UL
#define BATT_R_BOT_K            620UL
#define BATT_CUTOFF_MV          3300UL  /* below: LEDs disabled            */
#define BATT_RESUME_MV          3550UL  /* above: LEDs allowed again       */

/* ------------------------------------------------------------------------- */
/* Derived values (do not edit)                                              */
/* ------------------------------------------------------------------------- */
#define PWM_PERIOD_COUNTS       PWM_US_TO_COUNTS(PWM_PERIOD_US)

#if (PWM_PERIOD_COUNTS < 16UL) || (PWM_PERIOD_COUNTS > 65535UL)
#error "PWM_PERIOD_US out of range for the selected PWM clock / prescaler"
#endif

#define MV_TO_ADC(mv)           ((uint16_t)(((uint32_t)(mv) * 1023UL) / VDD_MV))
#define BATT_MV_TO_ADC(mv)      ((uint16_t)(((uint32_t)(mv) * BATT_R_BOT_K * 1023UL) / \
                                 ((BATT_R_TOP_K + BATT_R_BOT_K) * VDD_MV)))

#endif /* APP_CONFIG_H */
