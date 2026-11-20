/*
 * board.h - Pin map of the original U1 footprint (SOIC-8) and register bit
 *           masks used by hal.c. Bit masks are spelled out here instead of
 *           relying on the bit-field names in the XC8 device header.
 *
 *  Pin  Port  Board net                                   Use
 *   1   VDD   VCC 2.5 V (U2 regulator)                    supply
 *   2   RA5   PIR.a, digital PIR output                   input, IOC rising
 *   3   RA4   phototransistor ("CDS") + R1 330k || C3     AN3 light level
 *   4   RA3   button to GND                               MCLR (LVP build), or
 *                                                         input + WPU + IOC (HV build)
 *   5   RA2   unused on stock board -> night-light switch input + WPU
 *   6   RA1   R9 620R -> Q1..Q5 gates (R8 100k to GND)    PWM1 output
 *   7   RA0   B+ via R3 1.2M / R2 620k || C4              AN0 battery
 *   8   VSS   GND                                         ground
 */
#ifndef BOARD_H
#define BOARD_H

/* PORTA bit masks */
#define PIN_BATT_MASK       0x01u   /* RA0, pin 7, AN0 */
#define PIN_PWM_MASK        0x02u   /* RA1, pin 6, PWM1 (APFCON.P1SEL = 0) */
#define PIN_NLSW_MASK       0x04u   /* RA2, pin 5 */
#define PIN_BUTTON_MASK     0x08u   /* RA3, pin 4, MCLR in the LVP build */
#define PIN_LIGHT_MASK      0x10u   /* RA4, pin 3, AN3 */
#define PIN_PIR_MASK        0x20u   /* RA5, pin 2 */

/* ADC channels (ADCON0.CHS) */
#define ADC_CH_BATT         0u      /* AN0 */
#define ADC_CH_LIGHT        3u      /* AN3 */

/* OSCCON: IRCF<3:0> = 1101 (4 MHz HF), SCS<1:0> = 1x (internal oscillator) */
#define OSCCON_4MHZ         0x6Au

/* OPTION_REG */
#define OPTION_nWPUEN       0x80u

/* INTCON */
#define INTCON_IOCIE        0x08u

/* PIR1 */
#define PIR1_TMR2IF         0x02u

/* STATUS */
#define STATUS_nTO          0x10u

/* PCON (reset flags are active low, re-armed by writing 1) */
#define PCON_STKOVF         0x80u
#define PCON_STKUNF         0x40u
#define PCON_nRWDT          0x10u
#define PCON_nRMCLR         0x08u
#define PCON_nRI            0x04u
#define PCON_nPOR           0x02u
#define PCON_nBOR           0x01u
#define PCON_REARM          (PCON_nRWDT | PCON_nRMCLR | PCON_nRI | PCON_nPOR | PCON_nBOR)

/* WDTCON */
#define WDTCON_SWDTEN       0x01u

/* ADCON0 / ADCON1 */
#define ADCON0_ADON         0x01u
#define ADCON0_GO           0x02u
#define ADCON1_SETUP        0x90u   /* ADFM=1 right justified, ADCS=001 FOSC/8
                                       (2 us TAD @ 4 MHz), ADPREF=00 VDD */

/* T2CON: T2OUTPS = 1:5, TMR2ON, T2CKPS = 1:16 -> 1 MHz/16/125/5 = 100 Hz */
#define T2CON_10MS          0x26u
#define PR2_10MS            124u

/* PWM1CON */
#define PWMCON_EN           0x80u
#define PWMCON_OE           0x40u
/* PWM1LDCON */
#define PWMLDCON_LDA        0x80u

/* PMCON1 */
#define PMCON1_CFGS         0x40u
#define PMCON1_LWLO         0x20u
#define PMCON1_FREE         0x10u
#define PMCON1_WREN         0x04u
#define PMCON1_WR           0x02u
#define PMCON1_RD           0x01u

/* High-Endurance Flash: 0x780..0x7FF (low byte of each word). One 16-word
 * erase row at 0x780 holds the settings. The build reserves 0x780..0x7FF
 * from the linker (-mrom=default,-780-7ff). */
#define HEF_ROW_ADDR        0x0780u

#endif /* BOARD_H */
