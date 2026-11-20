/*
 * fade.c - Linear fade in PWM counts with 8 fractional bits.
 *
 * TODO: perceived brightness is not linear in duty; a gamma table would make
 * fades look more even (linear fades appear to rush at the dim end).
 */
#include <xc.h>
#include "app_config.h"
#include "hal.h"
#include "fade.h"

static uint16_t s_cur;          /* counts currently applied */
static uint16_t s_target;
static int32_t  s_acc;          /* s_cur << 8 during a fade */
static int32_t  s_step;         /* per-tick increment, << 8 */
static uint16_t s_ticks_left;

void fade_jump(uint16_t counts)
{
    s_cur = counts;
    s_target = counts;
    s_ticks_left = 0u;
    hal_pwm_set(counts);
}

void fade_to(uint16_t target)
{
    uint16_t ticks;

    if (target == s_target) {
        return;
    }
    s_target = target;

    ticks = (target > s_cur) ? MS_TO_TICKS(FADE_ON_MS) : MS_TO_TICKS(FADE_OFF_MS);
    if (ticks == 0u) {
        fade_jump(target);
        return;
    }

    s_acc = (int32_t)s_cur * 256;
    s_step = (((int32_t)target - (int32_t)s_cur) * 256) / (int32_t)ticks;
    s_ticks_left = ticks;
}

void fade_tick(void)
{
    if (s_ticks_left == 0u) {
        return;
    }
    s_ticks_left--;
    if (s_ticks_left == 0u) {
        s_cur = s_target;           /* land exactly, no rounding residue */
    } else {
        s_acc += s_step;
        s_cur = (uint16_t)(s_acc >> 8);
    }
    hal_pwm_set(s_cur);
}

uint16_t fade_current(void)
{
    return s_cur;
}

bool fade_busy(void)
{
    return s_ticks_left != 0u;
}
