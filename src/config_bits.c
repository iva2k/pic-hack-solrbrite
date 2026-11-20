/*
 * config_bits.c - PIC12LF1572 configuration words.
 *
 * LVP build (HV_PROGRAMMING_ONLY = 0, the default): LVP stays ON, because the
 * Raspberry Pi (Pickle) and the MPLAB Snap only program in low-voltage mode.
 * With LVP on, MCLRE is ignored and pin 4 is always MCLR with its weak
 * pull-up, so the button works through an MCLR reset (see main.c).
 *
 * HV build (HV_PROGRAMMING_ONLY = 1): LVP and MCLRE are OFF and pin 4 is the
 * RA3 input. LVP can't be cleared during low-voltage programming, so only a
 * high-voltage programmer can write this build or reprogram the chip after
 * it; tools\flash-pi.ps1 refuses such a HEX.
 */
#include <xc.h>
#include "app_config.h"

/* CONFIG1 */
#pragma config FOSC     = INTOSC    /* internal oscillator, I/O on CLKIN */
#pragma config WDTE     = SWDTEN    /* WDT controlled by WDTCON.SWDTEN */
#pragma config PWRTE    = ON        /* 64 ms power-up timer */
#if HV_PROGRAMMING_ONLY
#pragma config MCLRE    = OFF       /* pin 4 is RA3, the button input */
#else
#pragma config MCLRE    = ON        /* ignored while LVP = ON */
#endif
#pragma config CP       = OFF
#pragma config BOREN    = NSLEEP    /* BOR while running, off in Sleep */
#pragma config CLKOUTEN = OFF

/* CONFIG2 */
#pragma config WRT      = OFF       /* HEF writes need self-write access */
#pragma config PLLEN    = OFF
#pragma config STVREN   = ON
#pragma config BORV     = LO
#pragma config LPBOREN  = OFF
#if HV_PROGRAMMING_ONLY
#pragma config LVP      = OFF
#else
#pragma config LVP      = ON
#endif
