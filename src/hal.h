/*
 * hal.h - Hardware abstraction for the PIC12LF1572 light controller.
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* Reset causes reported by hal_init() */
#define RESET_POR       0x01u   /* power-on (or brown-out) */
#define RESET_MCLR      0x02u   /* button press on pin 4   */
#define RESET_WDT       0x04u   /* watchdog while awake (firmware hang) */
#define RESET_OTHER     0x08u   /* RESET instruction, stack over/underflow */

/* Wake-up reasons reported by hal_sleep() */
#define WAKE_WDT        0x01u
#define WAKE_PIR        0x02u

/* Initialise clock, pins, timer tick, ADC, PWM, WDT. Returns RESET_* flags
 * and re-arms the PCON reset flags. */
uint8_t  hal_init(void);

/* Block until the next 10 ms tick; also clears the watchdog. */
void     hal_tick_wait(void);

/* Busy-wait (keeps clearing the watchdog). */
void     hal_delay_ms(uint16_t ms);

/* PWM output: counts of high time per period, 0 = output forced low.
 * Values >= hal_pwm_period() give 100 %. */
void     hal_pwm_set(uint16_t counts);
uint16_t hal_pwm_period(void);

/* 10-bit ADC conversion against VDD. */
uint16_t hal_adc_read(uint8_t channel);

/* Night-light switch on pin 5: true when closed to GND. */
bool     hal_nightlight_switch_closed(void);

/* Motion: true if the PIR output is high now or a rising edge was latched
 * since the last call (the latch is cleared). */
bool     hal_pir_active(void);

#if HV_PROGRAMMING_ONLY
/* Mode button on pin 4, raw level (no debounce): true while pressed. */
bool     hal_button_pressed(void);
#endif

/* Sleep until the watchdog period expires, the PIR rises (if pir_wake) or,
 * in the HV build, the button is pressed. PWM must already be at 0.
 * Returns WAKE_* flags; a button wake isn't flagged, the next button poll
 * sees the press. */
uint8_t  hal_sleep(bool pir_wake);

/* High-Endurance Flash: settings byte with an inverted copy for validation.
 * Returns false if the stored copy is invalid (e.g. first boot). */
bool     hal_hef_load(uint8_t *value);
void     hal_hef_store(uint8_t value);

#endif /* HAL_H */
