/*
 * hal.c - Hardware abstraction for the PIC12LF1572 light controller.
 *
 * No interrupt handlers are used: the main loop polls the Timer2 flag, and
 * Sleep is woken by the watchdog or by interrupt-on-change (IOCIE set with
 * GIE clear wakes the core without vectoring).
 */
#include <xc.h>
#include "app_config.h"
#include "board.h"
#include "hal.h"

/* Pin 4 as a pulled-up input with a falling-edge (press) latch: HV build
 * only. In the LVP build pin 4 is MCLR and none of this applies. */
#if HV_PROGRAMMING_ONLY
#define BUTTON_INPUT_MASK   PIN_BUTTON_MASK
#else
#define BUTTON_INPUT_MASK   0x00u
#endif

static uint16_t s_pwm_period;       /* period in PWM counts */
static bool     s_pwm_on;           /* output enabled (OE) */

/* ------------------------------------------------------------------------- */
/* Init                                                                      */
/* ------------------------------------------------------------------------- */

static uint8_t reset_cause(void)
{
    uint8_t pcon = PCON;
    uint8_t cause = 0u;

    if ((pcon & (PCON_nPOR | PCON_nBOR)) != (PCON_nPOR | PCON_nBOR)) {
        cause |= RESET_POR;
    } else if ((pcon & PCON_nRMCLR) == 0u) {
        cause |= RESET_MCLR;
    }
    if ((pcon & PCON_nRWDT) == 0u) {
        cause |= RESET_WDT;
    }
    if (((pcon & PCON_nRI) == 0u) || ((pcon & (PCON_STKOVF | PCON_STKUNF)) != 0u)) {
        cause |= RESET_OTHER;
    }
    PCON = PCON_REARM;              /* re-arm all flags, clear stack flags */
    return cause;
}

static void pins_init(void)
{
    LATA   = 0x00u;                                 /* PWM pin low */
    ANSELA = PIN_BATT_MASK | PIN_LIGHT_MASK;        /* AN0, AN3 analog */
    /* Reset value is 0x3F! The button pull-up only draws current while the
     * button is held. */
    WPUA   = BUTTON_INPUT_MASK;
    TRISA  = (uint8_t)~PIN_PWM_MASK;                /* all inputs except PWM */
    OPTION_REG &= (uint8_t)~OPTION_nWPUEN;          /* allow per-pin WPU */
    APFCON = 0x00u;                                 /* PWM1 on RA1 (pin 6) */
    IOCAP  = PIN_PIR_MASK;          /* latch PIR rising edges while awake */
    IOCAN  = BUTTON_INPUT_MASK;
    IOCAF  = 0x00u;
}

static void tick_init(void)
{
    PR2   = PR2_10MS;
    TMR2  = 0u;
    PIR1 &= (uint8_t)~PIR1_TMR2IF;
    T2CON = T2CON_10MS;
}

static void pwm_init(void)
{
    s_pwm_period = (uint16_t)PWM_PERIOD_COUNTS;

    PWM1CON    = 0x00u;
    PWM1INTE   = 0x00u;
    PWM1CLKCON = (uint8_t)(PWM_PRESCALE_LOG2 << 4);   /* clock = FOSC */
    PWM1LDCON  = 0x00u;                                /* immediate reload */
    PWM1OFCON  = 0x00u;                                /* independent run */

    PWM1PHH = 0u;
    PWM1PHL = 0u;
    PWM1PRH = (uint8_t)((s_pwm_period - 1u) >> 8);
    PWM1PRL = (uint8_t)(s_pwm_period - 1u);
    PWM1DCH = 0u;
    PWM1DCL = 0u;
    PWM1LDCON = PWMLDCON_LDA;

    PWM1CON = PWMCON_EN;            /* running, output disabled -> LATA1 = 0 */
    s_pwm_on = false;
}

uint8_t hal_init(void)
{
    uint8_t cause = reset_cause();

    OSCCON = OSCCON_4MHZ;
    INTCON = 0x00u;                 /* no ISR; IOCIE is set only for Sleep */

    pins_init();

    ADCON1 = ADCON1_SETUP;
    ADCON0 = 0x00u;                 /* ADC off until needed */

    tick_init();
    pwm_init();

    WDTCON = (uint8_t)((WDT_PS_CODE << 1) | WDTCON_SWDTEN);
    CLRWDT();

    return cause;
}

/* ------------------------------------------------------------------------- */
/* Timing                                                                    */
/* ------------------------------------------------------------------------- */

void hal_tick_wait(void)
{
    while ((PIR1 & PIR1_TMR2IF) == 0u) {
        /* idle */
    }
    PIR1 &= (uint8_t)~PIR1_TMR2IF;
    CLRWDT();
}

void hal_delay_ms(uint16_t ms)
{
    while (ms != 0u) {
        __delay_ms(1);
        CLRWDT();
        ms--;
    }
}

/* ------------------------------------------------------------------------- */
/* PWM                                                                       */
/* ------------------------------------------------------------------------- */

uint16_t hal_pwm_period(void)
{
    return s_pwm_period;
}

void hal_pwm_set(uint16_t counts)
{
    uint16_t guard;

    if (counts == 0u) {
        PWM1CON = PWMCON_EN;        /* OE = 0: pin follows LATA1 = 0 */
        s_pwm_on = false;
        return;
    }

    if (counts > s_pwm_period) {
        counts = s_pwm_period;      /* DC > PR never matches -> 100 % */
    }

    PWM1DCH = (uint8_t)(counts >> 8);
    PWM1DCL = (uint8_t)counts;
    PWM1LDCON = PWMLDCON_LDA;       /* load at end of current period */

    if (!s_pwm_on) {
        /* Wait for the new duty to load before enabling the pin so a stale
         * duty from an earlier fade never flashes (max one period). */
        guard = 0xFFFFu;
        while (((PWM1LDCON & PWMLDCON_LDA) != 0u) && (guard != 0u)) {
            guard--;
        }
        PWM1CON = PWMCON_EN | PWMCON_OE;
        s_pwm_on = true;
    }
}

/* ------------------------------------------------------------------------- */
/* ADC                                                                       */
/* ------------------------------------------------------------------------- */

uint16_t hal_adc_read(uint8_t channel)
{
    uint16_t result;

    ADCON0 = (uint8_t)((channel << 2) | ADCON0_ADON);
    __delay_us(ADC_ACQ_US);
    ADCON0 |= ADCON0_GO;
    while ((ADCON0 & ADCON0_GO) != 0u) {
        /* ~12 TAD = 24 us */
    }
    result = (uint16_t)(((uint16_t)ADRESH << 8) | ADRESL);
    ADCON0 = 0x00u;                 /* ADC off */
    return result;
}

/* ------------------------------------------------------------------------- */
/* Digital inputs                                                            */
/* ------------------------------------------------------------------------- */

bool hal_nightlight_switch_closed(void)
{
    bool closed;

    WPUA |= PIN_NLSW_MASK;          /* pull-up only while reading */
    __delay_us(50);                 /* charge wiring capacitance */
    closed = ((PORTA & PIN_NLSW_MASK) == 0u);
    WPUA &= (uint8_t)~PIN_NLSW_MASK;
    return closed;
}

bool hal_pir_active(void)
{
    bool edge = ((IOCAF & PIN_PIR_MASK) != 0u);

    if (edge) {
        IOCAF &= (uint8_t)~PIN_PIR_MASK;
    }
    return edge || ((PORTA & PIN_PIR_MASK) != 0u);
}

#if HV_PROGRAMMING_ONLY
bool hal_button_pressed(void)
{
    return ((PORTA & PIN_BUTTON_MASK) == 0u);
}
#endif

/* ------------------------------------------------------------------------- */
/* Sleep                                                                     */
/* ------------------------------------------------------------------------- */

uint8_t hal_sleep(bool pir_wake)
{
    uint8_t wake = 0u;

    hal_pwm_set(0u);
    ADCON0 = 0x00u;

#if HV_PROGRAMMING_ONLY
    /* The button must always wake the core, so IOCIE stays set and the PIR
     * edge detector itself is off while PIR edges must not wake. */
    IOCAP = pir_wake ? PIN_PIR_MASK : 0x00u;
    IOCAF = 0x00u;
    INTCON = INTCON_IOCIE;
#else
    /* The PIR rising-edge latch (IOCAP) stays on; IOCIE decides whether an
     * edge also wakes the core. */
    IOCAF = 0x00u;
    INTCON = pir_wake ? INTCON_IOCIE : 0x00u;
#endif

    CLRWDT();
    SLEEP();
    NOP();

    INTCON = 0x00u;
#if HV_PROGRAMMING_ONLY
    IOCAP = PIN_PIR_MASK;
    IOCAF &= (uint8_t)~PIN_BUTTON_MASK;     /* main polls the level instead */
#endif
    if ((STATUS & STATUS_nTO) == 0u) {
        wake |= WAKE_WDT;
    }
    if (pir_wake && ((IOCAF & PIN_PIR_MASK) != 0u)) {
        wake |= WAKE_PIR;           /* flag left set for hal_pir_active() */
    } else {
        IOCAF = 0x00u;              /* drop edges seen while not armed */
    }

    /* Timer2 stops in Sleep; restart the tick period cleanly. */
    TMR2 = 0u;
    PIR1 &= (uint8_t)~PIR1_TMR2IF;
    return wake;
}

/* ------------------------------------------------------------------------- */
/* High-Endurance Flash                                                      */
/* ------------------------------------------------------------------------- */

static uint8_t hef_read(uint16_t addr)
{
    PMCON1 &= (uint8_t)~PMCON1_CFGS;
    PMADRH = (uint8_t)(addr >> 8);
    PMADRL = (uint8_t)addr;
    PMCON1 |= PMCON1_RD;
    NOP();
    NOP();
    return PMDATL;                  /* HEF: low byte only */
}

static void nvm_unlock(void)
{
    /* Required sequence; must not be interrupted (GIE is never set). */
    PMCON2 = 0x55u;
    PMCON2 = 0xAAu;
    PMCON1 |= PMCON1_WR;
    NOP();
    NOP();
}

bool hal_hef_load(uint8_t *value)
{
    uint8_t v = hef_read(HEF_ROW_ADDR);
    uint8_t n = hef_read(HEF_ROW_ADDR + 1u);

    if ((uint8_t)(v ^ n) != 0xFFu) {
        return false;
    }
    *value = v;
    return true;
}

void hal_hef_store(uint8_t value)
{
    CLRWDT();

    /* Erase the 16-word row at HEF_ROW_ADDR (CPU stalls ~2 ms) */
    PMCON1 &= (uint8_t)~(PMCON1_CFGS | PMCON1_LWLO);
    PMADRH = (uint8_t)(HEF_ROW_ADDR >> 8);
    PMADRL = (uint8_t)HEF_ROW_ADDR;
    PMCON1 |= PMCON1_FREE | PMCON1_WREN;
    nvm_unlock();

    /* Word 0: value (load latch only) */
    PMCON1 |= PMCON1_WREN | PMCON1_LWLO;
    PMADRL = (uint8_t)HEF_ROW_ADDR;
    PMDATH = 0x00u;
    PMDATL = value;
    nvm_unlock();

    /* Word 1: inverted copy, then write the latches to flash */
    PMCON1 &= (uint8_t)~PMCON1_LWLO;
    PMADRL = (uint8_t)(HEF_ROW_ADDR + 1u);
    PMDATH = 0x00u;
    PMDATL = (uint8_t)~value;
    nvm_unlock();

    PMCON1 &= (uint8_t)~PMCON1_WREN;
    CLRWDT();
}
