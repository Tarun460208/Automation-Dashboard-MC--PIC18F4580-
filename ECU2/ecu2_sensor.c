/*
 * File:   ecu2_sensor.c
 * ECU2 - Engine Node
 * Implements RPM and engine temperature reading via ADC,
 * and turn indicator control via digital keypad.
 *
 * RPM:      Potentiometer on AN4 → 0–8000 RPM
 * Eng Temp: Thermistor on AN6   → 30–130 °C
 * Indicator: SWITCH3 = LEFT, SWITCH4 = RIGHT (PORTC)
 */

#include "ecu2_sensor.h"
#include "adc.h"
#include "digital_keypad.h"

/* Indicator state variables — read by ISR for LED blinking */
volatile IndicatorStatus prev_ind_status = e_ind_off;
volatile IndicatorStatus cur_ind_status  = e_ind_off;
volatile unsigned char   led_state       = LED_OFF;

/*
 * get_rpm()
 * Reads 10-bit ADC from potentiometer on RPM_ADC_CHANNEL (CH4).
 * Maps 0–1023 → 0–8000 RPM (idle ~750, redline 8000).
 * Returns: engine RPM as uint16_t
 */
uint16_t get_rpm(void)
{
    uint16_t adc_val = read_adc(RPM_ADC_CHANNEL);

    /* scale: rpm = (adc_val * 8000) / 1023 */
    uint16_t rpm = (uint16_t)(((uint32_t)adc_val * 8000UL) / 1023UL);

    return rpm;
}

/*
 * get_engine_temp()
 * Reads 10-bit ADC from thermistor on ENG_TEMP_ADC_CHANNEL (CH6).
 * Maps 0–1023 → 30–130 °C  (range of 100 °C).
 * Returns: engine temperature in °C as uint16_t
 */
uint16_t get_engine_temp(void)
{
    uint16_t adc_val = read_adc(ENG_TEMP_ADC_CHANNEL);

    /* scale: temp = 30 + (adc_val * 100) / 1023 */
    uint16_t temp = 30U + (uint16_t)(((uint32_t)adc_val * 100UL) / 1023UL);

    return temp;
}

/*
 * process_indicator()
 * Polls SWITCH3 (LEFT indicator) and SWITCH4 (RIGHT indicator)
 * using STATE_CHANGE (edge-detect) mode.
 *
 * Toggle behaviour:
 *   - Press LEFT  while off      → activate left
 *   - Press LEFT  while left on  → turn off
 *   - Press RIGHT while off      → activate right
 *   - Press RIGHT while right on → turn off
 *   - Switching sides turns off the previous side first
 *
 * Updates cur_ind_status and led_state globals.
 * Physical LED macros (RIGHT_IND_ON/OFF, LEFT_IND_ON/OFF) control PORTB.
 * Returns: current IndicatorStatus enum value
 */
IndicatorStatus process_indicator(void)
{
    unsigned char key = read_digital_keypad(STATE_CHANGE);

    prev_ind_status = cur_ind_status;

    if (key == SWITCH3)
    {
        /* LEFT indicator button */
        if (cur_ind_status == e_ind_left)
        {
            /* Already left — toggle off */
            LEFT_IND_OFF();
            cur_ind_status = e_ind_off;
            led_state      = LED_OFF;
        }
        else
        {
            /* Turn off right if active, turn on left */
            RIGHT_IND_OFF();
            LEFT_IND_ON();
            cur_ind_status = e_ind_left;
            led_state      = LED_ON;
        }
    }
    else if (key == SWITCH4)
    {
        /* RIGHT indicator button */
        if (cur_ind_status == e_ind_right)
        {
            /* Already right — toggle off */
            RIGHT_IND_OFF();
            cur_ind_status = e_ind_off;
            led_state      = LED_OFF;
        }
        else
        {
            /* Turn off left if active, turn on right */
            LEFT_IND_OFF();
            RIGHT_IND_ON();
            cur_ind_status = e_ind_right;
            led_state      = LED_ON;
        }
    }

    return cur_ind_status;
}
