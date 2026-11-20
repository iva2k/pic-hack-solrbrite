/*
 * fade.h - Gradual brightness transitions on the PWM output.
 */
#ifndef FADE_H
#define FADE_H

#include <stdint.h>
#include <stdbool.h>

/* Start a transition to `target` counts. Brighter targets take FADE_ON_MS,
 * dimmer ones FADE_OFF_MS. Re-targeting mid-fade starts from the current
 * brightness. Calling with the current target does nothing. */
void     fade_to(uint16_t target);

/* Jump to `counts` immediately (no fade). */
void     fade_jump(uint16_t counts);

/* Advance one tick (call every TICK_MS while awake). */
void     fade_tick(void);

uint16_t fade_current(void);
bool     fade_busy(void);

#endif /* FADE_H */
