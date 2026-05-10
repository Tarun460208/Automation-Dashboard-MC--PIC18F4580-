/*
 * File:   ecu2_main.c
 * ECU2 - Engine Node (PIC18F4580)
 *
 * Responsibilities:
 *   - Read engine RPM from potentiometer via ADC (AN4)
 *   - Read engine temperature from thermistor via ADC (AN6)
 *   - Read indicator status from keypad (PORTC SW3/SW4)
 *   - Transmit all three values over CAN bus
 *
 * CAN Messages Transmitted:
 *   RPM_MSG_ID       (0x30) → 2 bytes, little-endian uint16_t (RPM)
 *   ENG_TEMP_MSG_ID  (0x40) → 2 bytes, little-endian uint16_t (°C)
 *   INDICATOR_MSG_ID (0x50) → 1 byte,  IndicatorStatus enum (0/1/2)
 *
 * Hardware:
 *   CAN TX  → RB2,  CAN RX → RB3
 *   RPM pot → AN4
 *   Temp    → AN6
 *   Keypad  → PORTC[3:0]  (SW3=LEFT, SW4=RIGHT)
 *   Ind LEDs→ PORTB[1:0] (LEFT), PORTB[7:6] (RIGHT)
 *   UART TX → RC6 (debug, 9600 baud)
 */

#include <xc.h>
#include "adc.h"
#include "can.h"
#include "digital_keypad.h"
#include "ecu2_sensor.h"
#include "msg_id.h"
#include "uart.h"

static void delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 1250; j++)
            continue;
}

int main(void)
{
    uint16_t        rpm, temp;
    IndicatorStatus ind;
    uint8_t         can_data[2];

    /* ---- Peripheral initialisation ---- */
    init_uart();
    init_adc();
    init_digital_keypad();

    /* Configure PORTB for indicator LEDs */
    TRISB = 0x08;   /* RB3 = input (CAN RX), rest = outputs */
    PORTB = 0x00;

    init_can();

    puts("ECU2 ENGINE NODE READY\r\n");

    /* ---- Main loop ---- */
    while (1)
    {
        /* --- RPM --- */
        rpm = get_rpm();

        can_data[0] = (uint8_t)(rpm & 0xFF);
        can_data[1] = (uint8_t)((rpm >> 8) & 0xFF);
        can_transmit(RPM_MSG_ID, can_data, 2);

        /* --- Engine Temperature --- */
        temp = get_engine_temp();

        can_data[0] = (uint8_t)(temp & 0xFF);
        can_data[1] = (uint8_t)((temp >> 8) & 0xFF);
        can_transmit(ENG_TEMP_MSG_ID, can_data, 2);

        /* --- Indicator --- */
        ind = process_indicator();

        can_data[0] = (uint8_t)ind;
        can_transmit(INDICATOR_MSG_ID, &can_data[0], 1);

        /* Debug UART output */
        puts("RPM:");
        putch('0' + (rpm / 1000) % 10);
        putch('0' + (rpm / 100)  % 10);
        putch('0' + (rpm / 10)   % 10);
        putch('0' + rpm          % 10);
        puts(" T:");
        putch('0' + (temp / 100) % 10);
        putch('0' + (temp / 10)  % 10);
        putch('0' + temp         % 10);
        puts("C IND:");
        putch('0' + (unsigned char)ind);
        puts("\r\n");

        delay_ms(20);   /* ~50 Hz update rate */
    }

    return 0;
}
