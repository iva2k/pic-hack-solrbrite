/*
 * main.c - Motion-only (optional night light) controller for the Costco
 *          #1806387 Fresh Source / Sol-R-Brite 5000 lm solar area light.
 *
 * Behaviour
 *   Day    : LEDs off, MCU sleeps, wakes every WDT_PERIOD_S to sample light.
 *   Night  : LEDs off (or night light if the pin-5 switch is closed); a PIR
 *            edge fades the LEDs up to the selected mode level for
 *            MOTION_HOLD_S after the last motion, then fades back down.
 *   Button : mode 1 -> 2 -> 3 -> OFF -> 1 on release, stored in HEF, and the
 *            new mode's brightness is shown for PREVIEW_S seconds. The LVP
 *            build sees the press as an MCLR reset; the HV build
 *            (HV_PROGRAMMING_ONLY) reads pin 4 as an input, without a reset.
 *   Battery: below BATT_CUTOFF_MV all LEDs are disabled until it recovers
 *            above BATT_RESUME_MV.
 *
 * All timing/threshold constants live in app_config.h.
 */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"
#include "board.h"
#include "hal.h"
#include "fade.h"

/* Modes (stored in HEF). Mode n uses brightness level n. */
#define MODE_OFF            0u
#define MODE_MAX            3u

/* Brightness level 0 = night light */
#define LEVEL_NIGHT         0u

#define CLAMP_COUNTS(us)    ((uint16_t)((PWM_US_TO_COUNTS(us) > PWM_PERIOD_COUNTS) ? \
                             PWM_PERIOD_COUNTS : PWM_US_TO_COUNTS(us)))

static const uint16_t k_level_counts[4] = {
    CLAMP_COUNTS(LEVEL0_HIGH_US),
    CLAMP_COUNTS(LEVEL1_HIGH_US),
    CLAMP_COUNTS(LEVEL2_HIGH_US),
    CLAMP_COUNTS(LEVEL3_HIGH_US),
};

#define LIGHT_NIGHT_ADC     MV_TO_ADC(LIGHT_NIGHT_MV)
#define LIGHT_DAY_ADC       MV_TO_ADC(LIGHT_DAY_MV)
#define BATT_CUTOFF_ADC     BATT_MV_TO_ADC(BATT_CUTOFF_MV)
#define BATT_RESUME_ADC     BATT_MV_TO_ADC(BATT_RESUME_MV)

#define SETTLE_TICKS        MS_TO_TICKS(LIGHT_SETTLE_MS)
#define SENSE_TICKS         ((uint16_t)(SENSE_PERIOD_S * TICKS_PER_S))
#define DEBOUNCE_TICKS      MS_TO_TICKS(BUTTON_DEBOUNCE_MS)

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */
static uint8_t  s_mode;             /* MODE_OFF or 1..3 */
static bool     s_night;            /* debounced day/night */
static uint8_t  s_light_votes;      /* consecutive samples against s_night */
static bool     s_batt_low;
static bool     s_nightlight_sw;    /* pin-5 switch closed */

static uint16_t s_preview_ticks;    /* >0: showing mode brightness */
static uint16_t s_hold_ticks;       /* >0: motion light on */
static uint16_t s_lockout_ticks;    /* >0: PIR ignored */
static uint8_t  s_warmup_s;         /* >0: PIR ignored after power-on */
static uint8_t  s_nl_sense_s;       /* countdown to next night-light blank */
static uint16_t s_off_ticks;        /* ticks since LEDs reached 0 (saturating) */
static uint16_t s_sense_ticks;      /* countdown to next periodic sense */
static uint8_t  s_sec_div;          /* ticks -> seconds */

#if HV_PROGRAMMING_ONLY
static bool     s_button_down;      /* debounced */
static uint8_t  s_button_settling_ticks;    /* ticks the raw level has differed */
#endif

/* ------------------------------------------------------------------------- */
/* Mode button                                                               */
/* ------------------------------------------------------------------------- */

/* Step to the next mode in the cycle 1 -> 2 -> 3 -> OFF -> 1, save it to HEF
 * (stalls the CPU ~2 ms) and start the preview. */
static void advance_mode(void)
{
    s_mode = (uint8_t)((s_mode + 1u) & 0x03u);
    hal_hef_store(s_mode);
    s_preview_ticks = (uint16_t)(PREVIEW_S * TICKS_PER_S);
}

#if HV_PROGRAMMING_ONLY
/* Debounce pin 4 once per tick. The mode advances on release, the same
 * moment the LVP build advances it (when MCLR is let go). */
static void poll_button(void)
{
    if (hal_button_pressed() == s_button_down) {
        s_button_settling_ticks = 0u;
        return;
    }
    if (++s_button_settling_ticks < DEBOUNCE_TICKS) {
        return;
    }
    s_button_settling_ticks = 0u;
    s_button_down = !s_button_down;
    if (!s_button_down) {
        advance_mode();
    }
}

/* True while a press is held or a level change is being debounced: the MCU
 * stays awake to see it through. */
static bool button_busy(void)
{
    return s_button_down || (s_button_settling_ticks != 0u);
}
#else
/* LVP build: the MCU is held in reset while the button is down. */
static bool button_busy(void)
{
    return false;
}
#endif

/* ------------------------------------------------------------------------- */
/* Sensing                                                                   */
/* ------------------------------------------------------------------------- */

static void update_light(uint16_t adc)
{
    bool against = s_night ? (adc > LIGHT_DAY_ADC) : (adc < LIGHT_NIGHT_ADC);

    if (!against) {
        s_light_votes = 0u;
        return;
    }
    if (++s_light_votes >= LIGHT_CONFIRM_SAMPLES) {
        s_night = !s_night;
        s_light_votes = 0u;
    }
}

static void update_battery(uint16_t adc)
{
    if (!s_batt_low && (adc < BATT_CUTOFF_ADC)) {
        s_batt_low = true;
    } else if (s_batt_low && (adc > BATT_RESUME_ADC)) {
        s_batt_low = false;
    }
}

/* Read the switch always; light and battery only when the LEDs have been off
 * long enough (no self-illumination, no load sag). */
static void sense_inputs(bool leds_settled_off)
{
    s_nightlight_sw = hal_nightlight_switch_closed();
    if (leds_settled_off) {
        update_light(hal_adc_read(ADC_CH_LIGHT));
        update_battery(hal_adc_read(ADC_CH_BATT));
    }
}

/* ------------------------------------------------------------------------- */
/* Decisions                                                                 */
/* ------------------------------------------------------------------------- */

static bool lights_allowed(void)
{
    return (s_mode != MODE_OFF) && s_night && !s_batt_low;
}

static bool pir_armed(void)
{
    return lights_allowed() && (s_warmup_s == 0u) &&
           (s_lockout_ticks == 0u) && (s_preview_ticks == 0u);
}

static uint16_t desired_counts(void)
{
    if (s_preview_ticks != 0u) {
        return (s_mode == MODE_OFF) ? 0u : k_level_counts[s_mode];
    }
    if (!lights_allowed()) {
        return 0u;
    }
    if (s_hold_ticks != 0u) {
        return k_level_counts[s_mode];
    }
    if (s_nightlight_sw) {
        return k_level_counts[LEVEL_NIGHT];
    }
    return 0u;
}

static void start_lockout(void)
{
    s_lockout_ticks = MS_TO_TICKS(FADE_OFF_MS + PIR_LOCKOUT_MS);
}

/* ------------------------------------------------------------------------- */
/* Per-tick work while awake                                                 */
/* ------------------------------------------------------------------------- */

static void tick(void)
{
    bool pir = hal_pir_active();    /* always consume the edge latch */

#if HV_PROGRAMMING_ONLY
    poll_button();
#endif

    /* seconds */
    if (++s_sec_div >= TICKS_PER_S) {
        s_sec_div = 0u;
        if (s_warmup_s != 0u)   { s_warmup_s--; }
        if (s_nl_sense_s != 0u) { s_nl_sense_s--; }
    }

    /* preview */
    if (s_preview_ticks != 0u) {
        if (--s_preview_ticks == 0u) {
            start_lockout();
        }
    }

    /* motion */
    if (s_lockout_ticks != 0u) {
        s_lockout_ticks--;
    }
    if (!lights_allowed()) {
        s_hold_ticks = 0u;
    } else if (pir && pir_armed()) {
        s_hold_ticks = (uint16_t)(MOTION_HOLD_S * TICKS_PER_S);
    } else if (s_hold_ticks != 0u) {
        if (--s_hold_ticks == 0u) {
            start_lockout();
        }
    }

    /* output */
    fade_to(desired_counts());
    fade_tick();

    if ((fade_current() == 0u) && !fade_busy()) {
        if (s_off_ticks != 0xFFFFu) { s_off_ticks++; }
    } else {
        s_off_ticks = 0u;
    }

    /* periodic sensing */
    if (--s_sense_ticks == 0u) {
        s_sense_ticks = SENSE_TICKS;
        sense_inputs(s_off_ticks >= SETTLE_TICKS);
    }

    /* Night light steady on: blank briefly to look for dawn. */
    if ((s_nl_sense_s == 0u) && (s_hold_ticks == 0u) && (s_preview_ticks == 0u) &&
        !fade_busy() && (fade_current() == k_level_counts[LEVEL_NIGHT]) &&
        (fade_current() != 0u)) {
        s_nl_sense_s = NIGHTLIGHT_SENSE_S;
        hal_pwm_set(0u);
        hal_delay_ms(LIGHT_SETTLE_MS);
        sense_inputs(true);
        hal_pwm_set(fade_current());    /* fade_to() handles any new target */
    }
}

static bool can_sleep(void)
{
    return (desired_counts() == 0u) && (fade_current() == 0u) && !fade_busy() &&
           (s_hold_ticks == 0u) && (s_lockout_ticks == 0u) &&
           (s_preview_ticks == 0u) && !button_busy();
}

static void sleep_once(void)
{
    bool pir_wake = lights_allowed() && (s_warmup_s == 0u);
    uint8_t wake = hal_sleep(pir_wake);

    if ((wake & WAKE_WDT) != 0u) {
        s_warmup_s = (s_warmup_s > WDT_PERIOD_S) ? (uint8_t)(s_warmup_s - WDT_PERIOD_S) : 0u;
        sense_inputs(true);             /* LEDs have been off for a WDT period */
    }
    s_sense_ticks = SENSE_TICKS;
}

/* ------------------------------------------------------------------------- */
/* Boot                                                                      */
/* ------------------------------------------------------------------------- */

static void boot(void)
{
    uint8_t cause = hal_init();
    uint8_t stored;

    if (!hal_hef_load(&stored) || (stored > MODE_MAX)) {
        stored = DEFAULT_MODE;
    }
    s_mode = stored;

#if HV_PROGRAMMING_ONLY
    /* A button already held at power-on doesn't count as a press. */
    s_button_down = hal_button_pressed();
#else
    if ((cause & RESET_MCLR) != 0u) {
        /* A bounce resets us again before this delay ends, so only the final
         * reset of a press/release sequence advances the mode. */
        hal_delay_ms(BUTTON_DEBOUNCE_MS);
        advance_mode();
    }
#endif

    s_warmup_s = ((cause & RESET_POR) != 0u) ? PIR_WARMUP_S : 0u;

    /* Initial day/night decision without vote delay (LEDs are off: R8 holds
     * the gates low through reset). */
    hal_delay_ms(LIGHT_SETTLE_MS);
    s_night = (hal_adc_read(ADC_CH_LIGHT) < LIGHT_NIGHT_ADC);
    s_light_votes = 0u;
    s_batt_low = (hal_adc_read(ADC_CH_BATT) < BATT_CUTOFF_ADC);
    s_nightlight_sw = hal_nightlight_switch_closed();

    s_nl_sense_s = NIGHTLIGHT_SENSE_S;
    s_sense_ticks = SENSE_TICKS;
    fade_jump(0u);
}

void main(void)
{
    boot();

    for (;;) {
        hal_tick_wait();
        tick();
        if (can_sleep()) {
            sleep_once();
        }
    }
}
