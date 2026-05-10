/*
 * File:   isr.h
 * ECU3 - Dashboard Node
 * ISR declaration for Timer0 blink and CAN RX interrupt.
 */

#ifndef ISR_H
#define ISR_H

void __interrupt() isr(void);

#endif /* ISR_H */
