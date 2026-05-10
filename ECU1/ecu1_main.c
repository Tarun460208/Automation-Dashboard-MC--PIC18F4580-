/*
 * File:   ecu1_main.c
 * ECU1 - Drive Node (PIC18F4580)
 *
 * Responsibilities:
 *   - Read vehicle speed from potentiometer via ADC (AN4)
 *   - Read gear position from digital keypad (PORTC)
 *   - Transmit both values over CAN bus every loop iteration
 *
 * CAN Messages Transmitted:
 *   SPEED_MSG_ID (0x10)  → 2 bytes, little-endian uint16_t (km/h)
 *   GEAR_MSG_ID  (0x20)  → 1 byte,  ASCII char ('N','R','1'–'6')
 *
 * Hardware:
 *   CAN TX → RB2,  CAN RX → RB3
 *   Speed pot → AN4 (RA4)
 *   Keypad    → PORTC[3:0]
 *   UART TX   → RC6  (debug, 9600 baud)
 */

#include <xc.h>
#include "adc.h"
#include "can.h"
#include "digital_keypad.h"
#include "ecu1_sensor.h"
#include "msg_id.h"
#include "uart.h"

/* Small busy-wait delay (~1 ms @ 20 MHz, 4 CPI) */
static void delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 1250; j++)
            continue;
}

int main(void)
{
    uint16_t      speed;
    unsigned char gear;
    uint8_t       can_data[2];

    /* ---- Peripheral initialisation ---- */
    init_uart();
    init_adc();
    init_digital_keypad();
    init_can();

    puts("ECU1 DRIVE NODE READY\r\n");

    /* ---- Main loop ---- */
    while (1)
    {
        /* --- Read speed sensor --- */
        speed = get_speed();            /* 0–200 km/h           */

        /* Pack speed as 2-byte little-endian */
        can_data[0] = (uint8_t)(speed & 0xFF);
        can_data[1] = (uint8_t)((speed >> 8) & 0xFF);

        /* Transmit speed on CAN */
        can_transmit(SPEED_MSG_ID, can_data, 2);

        /* --- Read gear position --- */
        gear = get_gear_pos();          /* 'N','R','1'–'6'      */

        can_data[0] = gear;
        can_transmit(GEAR_MSG_ID, &can_data[0], 1);

        /* Optional debug output */
        puts("SPD:");
        putch('0' + (speed / 100) % 10);
        putch('0' + (speed / 10)  % 10);
        putch('0' + speed         % 10);
        puts(" GR:");
        putch(gear);
        puts("\r\n");

        delay_ms(20);   /* ~50 Hz update rate */
    }

    return 0;
}
