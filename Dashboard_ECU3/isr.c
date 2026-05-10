/*
 * File:   isr.c
 * ECU3 - Dashboard Node
 *
 * Interrupt Service Routine:
 *   - Timer0 overflow → blink active indicator LEDs at ~1 Hz
 *   - CAN RX interrupt → set flag (polling handled in main loop)
 *
 * Timer0 is configured as 8-bit, Fosc/4, no prescaler, preloaded to 6.
 * Overflow period ≈ 250 * (4/20MHz) = 50 µs.
 * A software counter (TMR0_COUNT) counts to 20000 for a 1 s tick,
 * giving 0.5 Hz toggle → 1 Hz full blink cycle.
 */

#include <xc.h>
#include "message_handler.h"
#include "timer0.h"

/* Number of Timer0 overflows per blink toggle (~0.5 s) */
#define TMR0_RELOAD     6
#define BLINK_TICKS     10000U   /* 10000 × 50 µs = 500 ms per toggle */

static volatile unsigned int tmr0_count = 0;

void __interrupt() isr(void)
{
    /* ---- Timer0 overflow: indicator blink ---- */
    if (TMR0IE && TMR0IF)
    {
        TMR0   = TMR0_RELOAD;   /* Reload for next 250-tick interval */
        TMR0IF = 0;             /* Clear overflow flag               */

        tmr0_count++;

        if (tmr0_count >= BLINK_TICKS)
        {
            tmr0_count = 0;

            /* Toggle the active indicator side */
            if (led_state == LED_ON)
            {
                if (status == e_ind_left)
                {
                    /* Toggle left LEDs (PORTB[1:0]) */
                    if ((PORTB & 0x03) == 0x00)
                        LEFT_IND_ON();
                    else
                        LEFT_IND_OFF();
                }
                else if (status == e_ind_right)
                {
                    /* Toggle right LEDs (PORTB[7:6]) */
                    if ((PORTB & 0xC0) == 0x00)
                        RIGHT_IND_ON();
                    else
                        RIGHT_IND_OFF();
                }
            }
            else
            {
                /* Ensure both sides are off when led_state = OFF */
                LEFT_IND_OFF();
                RIGHT_IND_OFF();
            }
        }
    }

    /* ---- CAN RX interrupt (RXB0, mode 0) ---- */
    /* Frame is left in RXB0 — can_receive() polls RXB0FUL in main loop */
    if (RXB0IE && RXB0IF)
    {
        RXB0IF = 0;   /* Acknowledge; data read in process_canbus_data() */
    }
}
