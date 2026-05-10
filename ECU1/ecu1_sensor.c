/*
 * File:   ecu1_sensor.c
 * ECU1 - Drive Node
 * Implements speed reading via ADC and gear position via digital keypad.
 *
 * Speed:  Potentiometer on AN4 → 10-bit ADC → scaled to 0–200 km/h
 * Gear:   SWITCH1 (GEAR_UP), SWITCH2 (GEAR_DOWN) on PORTC
 *         SWITCH3 = COLLISION alert
 */

#include "ecu1_sensor.h"
#include "adc.h"

/* Current gear state: 0=N, 7=R, 1-6 = forward gears */
static unsigned char current_gear = 0;  /* Start in Neutral */

/*
 * get_speed()
 * Reads 10-bit ADC from potentiometer on SPEED_ADC_CHANNEL (CH4).
 * Maps 0–1023 → 0–200 km/h using integer arithmetic.
 * Returns: speed in km/h as uint16_t
 */
uint16_t get_speed(void)
{
    uint16_t adc_val = read_adc(SPEED_ADC_CHANNEL);

    /* Scale: speed = (adc_val * 200) / 1023 */
    /* Use 32-bit intermediate to avoid overflow */
    uint16_t speed = (uint16_t)(((uint32_t)adc_val * 200UL) / 1023UL);

    return speed;
}

/*
 * get_gear_pos()
 * Polls the digital keypad in STATE_CHANGE mode (edge detect).
 * GEAR_UP   (SW1) → increment gear, clamp at MAX_GEAR (6)
 * GEAR_DOWN (SW2) → decrement gear, clamp at Neutral (0)
 * COLLISION (SW3) → force Neutral (emergency)
 *
 * Gear encoding:
 *   0  → 'N' (Neutral)
 *   7  → 'R' (Reverse)
 *   1–6 → forward gears
 *
 * Returns: gear as unsigned char (ASCII 'N', 'R', or '1'–'6')
 */
unsigned char get_gear_pos(void)
{
    unsigned char key = read_digital_keypad(STATE_CHANGE);

    if (key == GEAR_UP)
    {
        /* SWITCH1 pressed: shift up */
        if (current_gear == 7)
        {
            /* Can't shift up from Reverse, go to Neutral */
            current_gear = 0;
        }
        else if (current_gear < MAX_GEAR)
        {
            current_gear++;
        }
        /* Already at MAX_GEAR: no change */
    }
    else if (key == GEAR_DOWN)
    {
        /* SWITCH2 pressed: shift down */
        if (current_gear > 1)
        {
            current_gear--;
        }
        else if (current_gear == 1)
        {
            current_gear = 0;   /* 1st → Neutral */
        }
        else if (current_gear == 0)
        {
            current_gear = 7;   /* Neutral → Reverse */
        }
    }
    else if (key == COLLISION)
    {
        /* SWITCH3: emergency — force Neutral */
        current_gear = 0;
    }

    /* Encode as ASCII for CAN transmission */
    if (current_gear == 0)  return 'N';
    if (current_gear == 7)  return 'R';
    return (unsigned char)('0' + current_gear);
}
