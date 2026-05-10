/*
 * File:   can.c
 * ECAN driver for PIC18F4580.
 * Supports Standard Frame (11-bit ID), Mode 0, 500 kbps @ 8 MHz.
 *
 * BUG FIX vs original:
 *   Original can_receive() unconditionally set *len = 0 after the
 *   if(RXB0FUL) block, discarding every received frame.
 *   Fixed: *len = 0 is set only when no frame is available.
 */

#include <xc.h>
#include "can.h"

typedef enum 
{
    e_can_op_mode_normal = 0x00,
    e_can_op_mode_loop   = 0x40,
    e_can_op_mode_config = 0x80
} CanOpMode;

void init_can(void)
{
    TRISB2 = 0;   /* CAN TX = RB2, output */
    TRISB3 = 1;   /* CAN RX = RB3, input  */

    CAN_SET_OPERATION_MODE_NO_WAIT(e_can_op_mode_config);
    while (CANSTAT != 0x80);   /* Wait for config mode */

    ECANCON = 0x00;            /* Mode 0 (legacy 2-buffer) */

    /*
     * Bit timing for 500 kbps @ Fosc = 8 MHz
     * TQ = 2 * (BRP+1) / Fosc = 2 * 2 / 8M = 500 ns
     * Bit time = (1 + 4 + 4) TQ = 9 TQ  →  ~500 kbps
     */
    BRGCON1 = 0xE1;  /* SJW = 4TQ,  BRP = 1 (÷4)   */
    BRGCON2 = 0x1B;  /* SEG2PHTS=1, PS1=4TQ, Prop=4TQ */
    BRGCON3 = 0x03;  /* PS2 = 4TQ                   */

    RXFCON0 = 0x00;  /* Disable acceptance filters (receive all) */

    CAN_SET_OPERATION_MODE_NO_WAIT(e_can_op_mode_normal);
    while ((CANSTAT & 0xE0) != 0x00);   /* Wait for normal mode */

    RXB0CON = 0x00;  /* Receive any message in RXB0 */
}

/* Pack 11-bit standard ID into TXB0SIDH:TXB0SIDL */
static void set_msg_id_std(unsigned int id)
{
    TXB0SIDL = (id & 0x07) << 5;   /* ID[2:0]  → SIDL[7:5] */
    TXB0SIDH = (id >> 3);           /* ID[10:3] → SIDH[7:0] */
}

/* Unpack 11-bit standard ID from RXB0SIDH:RXB0SIDL */
static uint16_t get_msg_id_std(void)
{
    return ((uint16_t)(RXB0SIDL >> 5) & 0x07) | ((uint16_t)RXB0SIDH << 3);
}

/*
 * can_transmit()
 * Loads msg_id, data bytes, and DLC into TXB0, then sets TXB0REQ.
 * len must be 0–8. No error handling for len > 8.
 */
void can_transmit(uint16_t msg_id, const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t *ptr;

    TXB0EIDH = 0x00;
    TXB0EIDL = 0x00;
    set_msg_id_std(msg_id);
    TXB0DLC = len;

    ptr = (uint8_t *)&TXB0D0;
    for (i = 0; i < len; i++)
        ptr[i] = data[i];

    TXB0REQ = 1;   /* Request transmission */

    /* Optional: wait for TX to complete */
    while (TXB0REQ);
}

/*
 * can_receive()
 * Polls RXB0FUL. If a frame is waiting, copies msg_id, data, and len
 * to caller. Clears RXB0FUL and RXB0IF before returning.
 * If no frame is available, sets *len = 0.
 *
 * BUG FIX: original set *len = 0 unconditionally after the block.
 */
void can_receive(uint16_t *msg_id, uint8_t *data, uint8_t *len)
{
    uint8_t i;
    uint8_t *ptr;

    if (RXB0FUL)
    {
        *msg_id = get_msg_id_std();
        *len    = RXB0DLC & 0x0F;   /* DLC is lower nibble */

        ptr = (uint8_t *)&RXB0D0;
        for (i = 0; i < *len; i++)
            data[i] = ptr[i];

        RXB0FUL = 0;   /* Clear buffer-full flag  */
        RXB0IF  = 0;   /* Clear interrupt flag     */

        return;        /* *len is already set      */
    }

    /* No frame available */
    *len = 0;
}
