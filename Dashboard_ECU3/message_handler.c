/*
 * File:   message_handler.c
 * ECU3 - Dashboard Node
 *
 * Receives CAN frames from ECU1 and ECU2, decodes payload,
 * updates the 16×2 CLCD display, and controls indicator LEDs.
 *
 * LCD Layout (16 chars per line):
 *   Line 1: "SPD:xxx GR:x    "    (speed km/h + gear)
 *   Line 2: "RPM:xxxx T:xxxC "    (RPM + engine temp)
 *
 * LED Blinking is handled by the Timer0 ISR in isr.c via
 * the led_state and status globals defined here.
 */

#include <xc.h>
#include <string.h>
#include "message_handler.h"
#include "msg_id.h"
#include "can.h"
#include "clcd.h"

/* ---- Globals (extern'd in message_handler.h) ---- */
volatile unsigned char led_state = LED_OFF;
volatile unsigned char status    = e_ind_off;

/* ---- Internal LCD line buffers ---- */
static unsigned char line1[17] = "SPD:000 GR:N    ";
static unsigned char line2[17] = "RPM:0000 T:030C ";

/* ---- Utility: write a decimal number into a buffer ----
 *  dst   : pointer to first digit position in buffer
 *  val   : value to write
 *  width : number of digit characters to fill (zero-padded)
 */
static void write_decimal(unsigned char *dst, uint16_t val, unsigned char width)
{
    unsigned char i;
    for (i = width; i > 0; i--)
    {
        dst[i - 1] = '0' + (val % 10);
        val /= 10;
    }
}

/*
 * handle_speed_data()
 * Decodes 2-byte little-endian speed (km/h) from ECU1.
 * Updates positions 4–6 of Line 1 on the LCD.
 *
 * data[0] = speed low byte
 * data[1] = speed high byte
 */
void handle_speed_data(uint8_t *data, uint8_t len)
{
    uint16_t speed;

    if (len < 2) return;

    speed = (uint16_t)data[0] | ((uint16_t)data[1] << 8);

    if (speed > 200) speed = 200;   /* Clamp to valid range */

    /* Line 1: "SPD:xxx GR:x    " positions 4,5,6 = speed digits */
    write_decimal(&line1[4], speed, 3);

    clcd_print(line1, LINE1(0));
}

/*
 * handle_gear_data()
 * Decodes 1-byte ASCII gear character from ECU1.
 * Updates position 11 of Line 1 on the LCD.
 *
 * data[0] = gear ASCII: 'N', 'R', '1'–'6'
 */
void handle_gear_data(uint8_t *data, uint8_t len)
{
    if (len < 1) return;

    unsigned char g = data[0];

    /* Validate: only accept legal gear values */
    if (g != 'N' && g != 'R' && !(g >= '1' && g <= '6'))
        g = 'N';

    /* Line 1: "SPD:xxx GR:x    " position 11 = gear char */
    line1[11] = g;

    clcd_print(line1, LINE1(0));
}

/*
 * handle_rpm_data()
 * Decodes 2-byte little-endian RPM from ECU2.
 * Updates positions 4–7 of Line 2 on the LCD.
 *
 * data[0] = RPM low byte
 * data[1] = RPM high byte
 */
void handle_rpm_data(uint8_t *data, uint8_t len)
{
    uint16_t rpm;

    if (len < 2) return;

    rpm = (uint16_t)data[0] | ((uint16_t)data[1] << 8);

    if (rpm > 8000) rpm = 8000;

    /* Line 2: "RPM:xxxx T:xxxC " positions 4,5,6,7 = RPM digits */
    write_decimal(&line2[4], rpm, 4);

    clcd_print(line2, LINE2(0));
}

/*
 * handle_engine_temp_data()
 * Decodes 2-byte little-endian temperature (°C) from ECU2.
 * Updates positions 11–13 of Line 2. Triggers warning if > 105 °C.
 *
 * data[0] = temp low byte
 * data[1] = temp high byte
 */
void handle_engine_temp_data(uint8_t *data, uint8_t len)
{
    uint16_t temp;

    if (len < 2) return;

    temp = (uint16_t)data[0] | ((uint16_t)data[1] << 8);

    if (temp > 130) temp = 130;
    if (temp < 30)  temp = 30;

    /* Line 2: "RPM:xxxx T:xxxC " positions 11,12,13 = temp digits */
    write_decimal(&line2[11], temp, 3);

    clcd_print(line2, LINE2(0));

    /* Over-temperature warning: blink all LEDs using led_state */
    if (temp > 105)
    {
        led_state = LED_ON;   /* ISR will blink via timer */
    }
}

/*
 * handle_indicator_data()
 * Decodes 1-byte IndicatorStatus enum from ECU2.
 * Sets the global 'status' variable consumed by the Timer0 ISR
 * to blink PORTB[1:0] (LEFT) or PORTB[7:6] (RIGHT).
 *
 * data[0]: 0 = off, 1 = left, 2 = right
 */
void handle_indicator_data(uint8_t *data, uint8_t len)
{
    if (len < 1) return;

    unsigned char new_status = data[0];

    if (new_status > 2) return;    /* Ignore invalid values */

    /* Turn off whichever side was previously active */
    if (status == e_ind_left)   LEFT_IND_OFF();
    if (status == e_ind_right)  RIGHT_IND_OFF();

    status = new_status;

    if (status == e_ind_off)
    {
        led_state = LED_OFF;
        LEFT_IND_OFF();
        RIGHT_IND_OFF();
    }
    else
    {
        led_state = LED_ON;    /* Arm Timer0 ISR blinking */
    }
}

/*
 * process_canbus_data()
 * Main polling function called from the while(1) loop.
 * Calls can_receive(); if a frame arrived (len > 0) dispatches
 * to the appropriate handler based on msg_id from msg_id.h.
 */
void process_canbus_data(void)
{
    uint16_t msg_id = 0;
    uint8_t  data[8];
    uint8_t  len = 0;

    can_receive(&msg_id, data, &len);

    if (len == 0) return;    /* No frame available */

    switch (msg_id)
    {
        case SPEED_MSG_ID:
            handle_speed_data(data, len);
            break;

        case GEAR_MSG_ID:
            handle_gear_data(data, len);
            break;

        case RPM_MSG_ID:
            handle_rpm_data(data, len);
            break;

        case ENG_TEMP_MSG_ID:
            handle_engine_temp_data(data, len);
            break;

        case INDICATOR_MSG_ID:
            handle_indicator_data(data, len);
            break;

        default:
            /* Unknown message ID — silently ignore */
            break;
    }
}
