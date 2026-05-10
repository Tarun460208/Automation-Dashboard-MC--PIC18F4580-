/* Author :Tarun Gaurav
 * File:   main.c
 * ECU3 - Dashboard Node (PIC18F4580)
 *
 * Receives CAN messages from ECU1 and ECU2, updates the
 * 16×2 CLCD display, and blinks indicator LEDs via Timer0 ISR.
 *
 * LCD Line 1: "SPD:xxx GR:x    "
 * LCD Line 2: "RPM:xxxx T:xxxC "
 *
 * CAN Messages Received:
 *   0x10 SPEED_MSG_ID      → handle_speed_data()
 *   0x20 GEAR_MSG_ID       → handle_gear_data()
 *   0x30 RPM_MSG_ID        → handle_rpm_data()
 *   0x40 ENG_TEMP_MSG_ID   → handle_engine_temp_data()
 *   0x50 INDICATOR_MSG_ID  → handle_indicator_data()
 *
 * Hardware:
 *   CAN TX/RX → RB2/RB3
 *   LCD data  → PORTD,  control → RC[2:0]
 *   Ind LEDs  → PORTB[1:0] (LEFT), PORTB[7:6] (RIGHT)
 *   Timer0    → indicator blink ISR
 */

#include <xc.h>
#include <stdint.h>
#include "can.h"
#include "clcd.h"
#include "isr.h"
#include "message_handler.h"
#include "msg_id.h"
#include "timer0.h"

/* Configure indicator LED pins on PORTB */
static void init_leds(void)
{
    TRISB = 0x08;   /* RB3 = input (CAN RX), all others = output */
    PORTB = 0x00;   /* All LEDs off on startup */
}

static void init_config(void)
{
    init_clcd();    /* 16×2 HD44780 LCD on PORTD + RC[2:0] */
    init_can();     /* ECAN @ 500 kbps on RB2 (TX) / RB3 (RX) */
    init_leds();    /* Indicator LEDs on PORTB */

    /* Enable interrupts for Timer0 blink ISR */
    PEIE = 1;       /* Peripheral interrupt enable */
    GIE  = 1;       /* Global interrupt enable     */

    init_timer0();  /* Timer0: 50 µs tick for indicator blink */
}

void main(void)
{
    init_config();

    /* Display startup message for 1 s */
    clcd_print((const unsigned char *)"  CAN DASHBOARD ", LINE1(0));
    clcd_print((const unsigned char *)"   INITIALISED  ", LINE2(0));

    /* Rough 1-second delay */
    {
        unsigned int i, j;
        for (i = 0; i < 1000; i++)
            for (j = 0; j < 1250; j++)
                continue;
    }

    /* Clear display and show live data template */
    clcd_print((const unsigned char *)"SPD:000 GR:N    ", LINE1(0));
    clcd_print((const unsigned char *)"RPM:0000 T:030C ", LINE2(0));

    /* Main receive and display loop */
    while (1)
    {
        process_canbus_data();
    }
}
