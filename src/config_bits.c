/*
 * config_bits.c - PIC12LF1572 configuration words.
 *
 * LVP must stay ON: the Raspberry Pi (Pickle) and MPLAB Snap can only program
 * in low-voltage mode. With LVP on, MCLRE is ignored and pin 4 is always MCLR
 * with its weak pull-up - the button works through MCLR reset (see main.c).
 * Programming LVP = OFF would require a high-voltage programmer to recover.
 */
#include <xc.h>

/* CONFIG1 */
#pragma config FOSC     = INTOSC    /* internal oscillator, I/O on CLKIN */
#pragma config WDTE     = SWDTEN    /* WDT controlled by WDTCON.SWDTEN */
#pragma config PWRTE    = ON        /* 64 ms power-up timer */
#pragma config MCLRE    = ON        /* ignored while LVP = ON */
#pragma config CP       = OFF
#pragma config BOREN    = NSLEEP    /* BOR while running, off in Sleep */
#pragma config CLKOUTEN = OFF

/* CONFIG2 */
#pragma config WRT      = OFF       /* HEF writes need self-write access */
#pragma config PLLEN    = OFF
#pragma config STVREN   = ON
#pragma config BORV     = LO
#pragma config LPBOREN  = OFF
#pragma config LVP      = ON
