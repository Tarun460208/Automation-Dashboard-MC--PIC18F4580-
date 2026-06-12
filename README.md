# CAN Automotive Dashboard — 3-ECU System

A PIC18F4580-based automotive dashboard system using the ECAN peripheral at 500 kbps. Three independent ECUs communicate over a shared CAN bus — ECU1 reads drive sensors, ECU2 reads engine sensors, and ECU3 (Dashboard) receives all data and displays it on a 16×2 LCD with blinking indicator LEDs.

**Author:** Tarun Gaurav
**MCU:** PIC18F4580 (×3 nodes)
**Toolchain:** MPLAB X + XC8 compiler
**CAN Speed:** 500 kbps, Standard Frame (11-bit ID), Mode 0

---

## Table of Contents

- [Project Structure](#project-structure)
- [System Architecture](#system-architecture)
- [CAN Message Map](#can-message-map)
- [Flow Diagrams](#flow-diagrams)
- [ECU1 — Drive Node](#ecu1--drive-node)
- [ECU2 — Engine Node](#ecu2--engine-node)
- [ECU3 — Dashboard Node](#ecu3--dashboard-node)
- [Hardware Pinout](#hardware-pinout)
- [Demo Output](#demo-output)
- [Build](#build)
- [File Reference](#file-reference)

---

## Project Structure

```
CAN_AUTOMOTIVE_PROJECT_1/
│
├── ECU1/                        →  Drive Node (speed + gear)
│   ├── ecu1_main.c              →  main(), peripheral init, TX loop
│   ├── ecu1_sensor.c/.h         →  get_speed(), get_gear_pos()
│   ├── adc.c/.h                 →  10-bit ADC driver
│   ├── digital_keypad.c/.h      →  PORTC keypad (LEVEL / STATE_CHANGE)
│   ├── can.c/.h                 →  ECAN driver (transmit + receive)
│   ├── uart.c/.h                →  UART debug (9600 baud)
│   └── msg_id.h                 →  Shared CAN message ID defines
│
├── ECU2/                        →  Engine Node (RPM + temp + indicator)
│   ├── ecu2_main.c              →  main(), peripheral init, TX loop
│   ├── ecu2_sensor.c/.h         →  get_rpm(), get_engine_temp(), process_indicator()
│   ├── adc.c/.h                 →  10-bit ADC driver
│   ├── digital_keypad.c/.h      →  PORTC keypad
│   ├── can.c/.h                 →  ECAN driver
│   ├── uart.c/.h                →  UART debug
│   └── msg_id.h                 →  Shared CAN message ID defines
│
└── Dashboard_ECU3/              →  Dashboard Node (receive + display)
    ├── main.c                   →  main(), init, while(1) → process_canbus_data()
    ├── message_handler.c/.h     →  CAN frame dispatcher + LCD update handlers
    ├── isr.c/.h                 →  Timer0 ISR: indicator LED blink + CAN RX ack
    ├── timer0.c/.h              →  Timer0 init (8-bit, Fosc/4, no prescaler)
    ├── clcd.c/.h                →  HD44780 16×2 LCD driver (PORTD + RC[2:0])
    ├── can.c/.h                 →  ECAN driver (receive only in dashboard context)
    ├── uart.c/.h                →  UART (unused on ECU3, retained for parity)
    └── msg_id.h                 →  Shared CAN message ID defines
```

---

## System Architecture

```
                    ┌──────────────────────────────────────────────────┐
                    │               CAN Bus (500 kbps)                 │
                    │                                                  │
     ┌──────────────┤◄────── TX ──────┬──────────── TX ──────────────► │
     │              │                 │                                │
     │   ECU3        │   ECU1          │   ECU2                        │
     │  Dashboard    │  Drive Node     │  Engine Node                  │
     │  PIC18F4580   │  PIC18F4580     │  PIC18F4580                   │
     │               │                 │                               │
     │  RX only      │  TX: 0x10,0x20  │  TX: 0x30,0x40,0x50          │
     │               │                 │                               │
     │  ┌─────────┐  │  ┌───────────┐  │  ┌───────────────────┐       │
     │  │16×2 LCD │  │  │ Speed pot │  │  │ RPM pot (AN4)     │       │
     │  │PORTD    │  │  │ AN4→ADC   │  │  │ Temp sensor (AN6) │       │
     │  └─────────┘  │  └───────────┘  │  │ Indicator SW3/SW4 │       │
     │               │  ┌───────────┐  │  └───────────────────┘       │
     │  ┌─────────┐  │  │ Gear keys │  │                               │
     │  │Ind LEDs │  │  │ SW1/SW2/  │  │  ┌───────────────────┐       │
     │  │PORTB    │  │  │ SW3 PORTC │  │  │ Ind LEDs PORTB    │       │
     │  └─────────┘  │  └───────────┘  │  └───────────────────┘       │
     └───────────────┴─────────────────┴───────────────────────────────┘
```

---

## CAN Message Map

All frames use **Standard Frame format (11-bit ID)**, 500 kbps, ECAN Mode 0.

| Message ID | Define            | Transmitter | Bytes | Payload                                     |
|------------|-------------------|-------------|-------|---------------------------------------------|
| `0x10`     | `SPEED_MSG_ID`    | ECU1        | 2     | `[D0, D1]` = little-endian uint16 km/h      |
| `0x20`     | `GEAR_MSG_ID`     | ECU1        | 1     | `[D0]` = ASCII: `'N'`, `'R'`, `'1'`–`'6'`  |
| `0x30`     | `RPM_MSG_ID`      | ECU2        | 2     | `[D0, D1]` = little-endian uint16 RPM       |
| `0x40`     | `ENG_TEMP_MSG_ID` | ECU2        | 2     | `[D0, D1]` = little-endian uint16 °C        |
| `0x50`     | `INDICATOR_MSG_ID`| ECU2        | 1     | `[D0]` = `0` off, `1` left, `2` right       |

---

## Flow Diagrams

### Overall System Flow

```
  ┌──────────────────────────────────────────────────────────────────┐
  │   ECU1 (Drive Node)           ECU2 (Engine Node)                │
  │                                                                  │
  │  init_uart()                 init_uart()                        │
  │  init_adc()                  init_adc()                         │
  │  init_digital_keypad()       init_digital_keypad()              │
  │  init_can()                  init_can()                         │
  │         │                           │                           │
  │         ▼                           ▼                           │
  │      while(1)                    while(1)                       │
  │         │                           │                           │
  │    ┌────┴────┐                 ┌────┴────┐                      │
  │    │get_speed│                 │get_rpm()│                      │
  │    │  ADC AN4│                 │  ADC AN4│                      │
  │    │0–200km/h│                 │0–8000RPM│                      │
  │    └────┬────┘                 └────┬────┘                      │
  │         │can_transmit(0x10,2)       │can_transmit(0x30,2)       │
  │         ▼                           ▼                           │
  │    ┌─────────┐               ┌────────────┐                     │
  │    │get_gear │               │get_eng_temp│                     │
  │    │  PORTC  │               │   ADC AN6  │                     │
  │    │SW1/SW2/ │               │  30–130°C  │                     │
  │    │  SW3    │               └─────┬──────┘                     │
  │    └────┬────┘                     │can_transmit(0x40,2)        │
  │         │can_transmit(0x20,1)      ▼                            │
  │         │                   ┌─────────────┐                     │
  │         │                   │process_ind()│                     │
  │         │                   │SW3=LEFT     │                     │
  │         │                   │SW4=RIGHT    │                     │
  │         │                   └──────┬──────┘                     │
  │         │                          │can_transmit(0x50,1)        │
  │         │                          │                            │
  └─────────┼──────────────────────────┼────────────────────────────┘
            │                          │
            └──────────┬───────────────┘
                       │ CAN Bus (500 kbps)
                       ▼
  ┌────────────────────────────────────────────────────────────────┐
  │                   ECU3 (Dashboard Node)                        │
  │                                                                │
  │  init_clcd() · init_can() · init_leds() · init_timer0()       │
  │  Enable GIE + PEIE                                             │
  │         │                                                      │
  │         ▼                                                      │
  │   Display "CAN DASHBOARD INITIALISED" → 1s delay              │
  │         │                                                      │
  │         ▼                                                      │
  │      while(1)                                                  │
  │         │                                                      │
  │         ▼                                                      │
  │   process_canbus_data()                                        │
  │         │                                                      │
  │   can_receive(&id, data, &len)                                 │
  │         │                                                      │
  │   len==0 ──► return (no frame)                                 │
  │         │                                                      │
  │         ▼                                                      │
  │     switch(msg_id)                                             │
  │         │                                                      │
  │   ┌─────┼──────┬──────┬──────┬──────┐                         │
  │  0x10  0x20  0x30  0x40  0x50  default                        │
  │   │     │     │     │     │    (ignore)                        │
  │   ▼     ▼     ▼     ▼     ▼                                    │
  │  spd  gear  rpm  temp   ind                                    │
  │  LCD  LCD   LCD  LCD    arm                                    │
  │  L1   L1    L2   L2     ISR                                    │
  │                                                                │
  │  ┌──────────────────────────────────────────────────────┐      │
  │  │  Timer0 ISR (every ~500 ms)                          │      │
  │  │  if led_state == LED_ON:                             │      │
  │  │    status==e_ind_left  → toggle PORTB[1:0]           │      │
  │  │    status==e_ind_right → toggle PORTB[7:6]           │      │
  │  └──────────────────────────────────────────────────────┘      │
  └────────────────────────────────────────────────────────────────┘
```

### ECU1 Gear State Machine

```
                  SW1 (GEAR_UP)               SW2 (GEAR_DOWN)
                  ┌──────────────────────────────────────────┐
                  │                                          │
    SW3(COLLISION)│                                          │
    ──────────────┤                                          │
    forces N      │                                          │
                  ▼                                          │
             ┌────────┐   SW1                ┌──────────┐   │
             │   N    │──────────────────────▶│    1     │   │
             │Neutral │◄─────────────────────│  1st Gr  │   │
             └───┬────┘   SW2                └────┬─────┘   │
                 │                                │ SW1      │
              SW2│                                ▼          │
                 ▼                           ┌──────────┐    │
             ┌────────┐                      │    2     │    │
             │   R    │                      │  2nd Gr  │    │
             │Reverse │                      └────┬─────┘    │
             └────────┘                           │  ...     │
               SW1→N                              ▼          │
                                             ┌──────────┐    │
                                             │    6     │    │
                                             │  6th Gr  │    │
                                             └──────────┘    │
                                                   │         │
                                                   └─────────┘
                                               SW1 clamps at 6
```

### ECU2 Indicator State Machine

```
           SW3 pressed          SW4 pressed
           (LEFT button)        (RIGHT button)
                │                    │
         ┌──────▼──────┐      ┌──────▼──────┐
         │ cur==left?  │      │ cur==right? │
         └──┬──────┬───┘      └──┬──────┬───┘
          yes      no           yes     no
           │       │             │      │
           ▼       ▼             ▼      ▼
        OFF()   LEFT_ON()    OFF()  RIGHT_ON()
        e_ind_  e_ind_left   e_ind_ e_ind_right
        off                  off
           │       │             │      │
           └───────┴─────────────┴──────┘
                        │
                        ▼
              can_transmit(0x50, {status}, 1)
                        │
                        ▼
             ECU3 ISR blinks PORTB LEDs
```

### ECU3 CAN Frame Dispatch

```
  can_receive()
       │
       ▼
  RXB0FUL==1?
   yes │        no
       │        └──► *len = 0 → return
       ▼
  read RXB0SIDH:SIDL → msg_id
  read RXB0DLC       → len
  copy RXB0D0..Dn    → data[]
  clear RXB0FUL, RXB0IF
       │
       ▼
  switch(msg_id)
  ┌────┬──────┬──────┬──────┬──────┐
  │0x10│ 0x20 │ 0x30 │ 0x40 │ 0x50 │
  └──┬─┴──┬───┴──┬───┴──┬───┴──┬───┘
     │    │      │      │      │
     ▼    ▼      ▼      ▼      ▼
  speed gear   rpm   temp    ind
  LE16  ASCII  LE16  LE16   enum
  →LCD1 →LCD1  →LCD2 →LCD2  →ISR
  [4:6] [11]  [4:7] [11:13] status
```

### Timer0 ISR Blink Logic

```
  Timer0 overflow (every 50 µs)
        │
        ▼
  Reload TMR0 = 6
  Clear TMR0IF
  tmr0_count++
        │
  tmr0_count >= 10000? (500 ms)
   no ──► return
  yes │
      ▼
  tmr0_count = 0
        │
  led_state == LED_ON?
   no ──► LEFT_IND_OFF(), RIGHT_IND_OFF()
  yes │
      ▼
  status == e_ind_left?
  ├── yes → toggle PORTB[1:0]
  └── status == e_ind_right?
       └── yes → toggle PORTB[7:6]
```

---

## ECU1 — Drive Node

**MCU:** PIC18F4580 | **Tx Rate:** ~50 Hz

### Responsibilities

Reads vehicle speed from a potentiometer and gear position from a digital keypad, then transmits both over CAN every 20 ms.

### Sensor Scaling

| Sensor       | ADC Channel | Raw Range | Scaled Output         |
|--------------|-------------|-----------|---------------------- |
| Speed pot    | AN4 (CH4)   | 0–1023    | 0–200 km/h (uint16)   |
| Gear keypad  | PORTC[3:0]  | SW1/SW2   | 'N','R','1'–'6' (ASCII)|

Speed formula: `speed = (adc_val × 200) / 1023`

### Files

| File                  | Role                                              |
|-----------------------|---------------------------------------------------|
| `ecu1_main.c`         | `main()`: init → TX loop (speed + gear every 20ms)|
| `ecu1_sensor.c/.h`    | `get_speed()`, `get_gear_pos()` implementations   |
| `adc.c/.h`            | `init_adc()`, `read_adc(channel)`                 |
| `digital_keypad.c/.h` | `init_digital_keypad()`, `read_digital_keypad()`  |
| `can.c/.h`            | `init_can()`, `can_transmit()`                    |
| `uart.c/.h`           | `init_uart()`, `putch()`, `puts()` (debug)        |
| `msg_id.h`            | `SPEED_MSG_ID 0x10`, `GEAR_MSG_ID 0x20`           |

---

## ECU2 — Engine Node

**MCU:** PIC18F4580 | **Tx Rate:** ~50 Hz

### Responsibilities

Reads engine RPM and temperature via ADC, reads turn indicator input from keypad, and transmits all three values over CAN every 20 ms. Controls indicator LEDs locally via PORTB.

### Sensor Scaling

| Sensor         | ADC Channel | Raw Range | Scaled Output          |
|----------------|-------------|-----------|------------------------|
| RPM pot        | AN4 (CH4)   | 0–1023    | 0–8000 RPM (uint16)    |
| Eng temp sensor| AN6 (CH6)   | 0–1023    | 30–130 °C (uint16)     |
| Indicator SW   | PORTC[3:0]  | SW3/SW4   | `IndicatorStatus` enum |

RPM formula: `rpm = (adc_val × 8000) / 1023`
Temp formula: `temp = 30 + (adc_val × 100) / 1023`

### IndicatorStatus Enum

```c
typedef enum {
    e_ind_off   = 0,   /* No indicator active    */
    e_ind_left  = 1,   /* Left indicator active  */
    e_ind_right = 2    /* Right indicator active */
} IndicatorStatus;
```

### Files

| File                  | Role                                                   |
|-----------------------|--------------------------------------------------------|
| `ecu2_main.c`         | `main()`: init → TX loop (rpm + temp + ind every 20ms) |
| `ecu2_sensor.c/.h`    | `get_rpm()`, `get_engine_temp()`, `process_indicator()`|
| `adc.c/.h`            | 10-bit ADC driver                                      |
| `digital_keypad.c/.h` | Keypad input (STATE_CHANGE edge detect)                |
| `can.c/.h`            | ECAN driver                                            |
| `uart.c/.h`           | Debug UART                                             |
| `msg_id.h`            | `RPM_MSG_ID 0x30`, `ENG_TEMP_MSG_ID 0x40`, `INDICATOR_MSG_ID 0x50` |

---

## ECU3 — Dashboard Node

**MCU:** PIC18F4580 | **Mode:** Receive only

### Responsibilities

Receives all 5 CAN message types, decodes payloads, updates the 16×2 LCD in real-time, and blinks indicator LEDs via a Timer0 ISR at 1 Hz.

### LCD Layout

```
┌─────────────────┐
│ SPD:120 GR:3    │   Line 1: speed (positions 4–6) + gear (position 11)
│ RPM:3500 T:087C │   Line 2: RPM (positions 4–7) + temp (positions 11–13)
└─────────────────┘
```

Over-temperature warning (>105 °C): `led_state` is set, Timer0 ISR blinks all LEDs.

### Indicator LED Mapping (PORTB)

| Side  | PORTB bits | Macro          |
|-------|------------|----------------|
| Left  | [1:0]      | `LEFT_IND_ON/OFF()`  |
| Right | [7:6]      | `RIGHT_IND_ON/OFF()` |

### Files

| File                    | Role                                                       |
|-------------------------|------------------------------------------------------------|
| `main.c`                | `main()`: init config, startup splash, while(1) loop       |
| `message_handler.c/.h`  | `process_canbus_data()`, 5 handler functions, LCD buffers  |
| `isr.c/.h`              | `__interrupt() isr()`: Timer0 blink + CAN RX ack           |
| `timer0.c/.h`           | `init_timer0()`: 8-bit, Fosc/4, preload=6, no prescaler    |
| `clcd.c/.h`             | HD44780 driver: `init_clcd()`, `clcd_print()`, `clcd_putch()`|
| `can.c/.h`              | ECAN driver: `init_can()`, `can_receive()`                 |
| `msg_id.h`              | All 5 message ID defines shared with ECU1/ECU2             |

---

## Hardware Pinout

### All ECUs — CAN Bus

| Signal  | Pin  |
|---------|------|
| CAN TX  | RB2  |
| CAN RX  | RB3  |

### ECU1

| Signal          | Pin        |
|-----------------|------------|
| Speed pot       | RA4 / AN4  |
| Gear UP (SW1)   | RC0        |
| Gear DOWN (SW2) | RC1        |
| Collision (SW3) | RC2        |
| UART TX (debug) | RC6        |

### ECU2

| Signal              | Pin        |
|---------------------|------------|
| RPM pot             | RA4 / AN4  |
| Engine temp sensor  | RA6 / AN6  |
| Left ind SW (SW3)   | RC2        |
| Right ind SW (SW4)  | RC3        |
| Left ind LEDs       | RB1, RB0   |
| Right ind LEDs      | RB7, RB6   |
| UART TX (debug)     | RC6        |

### ECU3 (Dashboard)

| Signal             | Pin            |
|--------------------|----------------|
| LCD data bus       | PORTD (RD0–RD7)|
| LCD EN             | RC2            |
| LCD RS             | RC1            |
| LCD RW             | RC0            |
| Left ind LEDs      | RB1, RB0       |
| Right ind LEDs     | RB7, RB6       |

---

## Demo Output

### ECU1 UART Debug (9600 baud)

```
ECU1 DRIVE NODE READY

SPD:045 GR:N
SPD:046 GR:N
SPD:047 GR:1       ← SW1 pressed (shift up to 1st)
SPD:051 GR:1
SPD:053 GR:2       ← SW1 pressed (shift up to 2nd)
SPD:075 GR:3
SPD:112 GR:3
SPD:119 GR:N       ← SW3 pressed (collision → force N)
```

### ECU2 UART Debug (9600 baud)

```
ECU2 ENGINE NODE READY

RPM:0750 T:035C IND:0
RPM:1200 T:038C IND:0
RPM:2500 T:052C IND:1    ← SW3 pressed (left indicator on)
RPM:3800 T:067C IND:1
RPM:3800 T:089C IND:0    ← SW3 pressed again (toggle off)
RPM:4100 T:106C IND:2    ← SW4 pressed (right on), over-temp!
```

### ECU3 LCD (16×2 HD44780)

**Normal operation:**
```
┌─────────────────┐
│ SPD:075 GR:3    │
│ RPM:3800 T:067C │
└─────────────────┘
```

**Startup splash (1 second):**
```
┌─────────────────┐
│  CAN DASHBOARD  │
│   INITIALISED   │
└─────────────────┘
```

**High speed, 6th gear:**
```
┌─────────────────┐
│ SPD:187 GR:6    │
│ RPM:7200 T:094C │
└─────────────────┘
```

**Over-temperature warning (>105 °C) + right indicator blinking:**
```
┌─────────────────┐
│ SPD:142 GR:4    │
│ RPM:4100 T:108C │   ← T > 105°C: LEDs blink
└─────────────────┘

PORTB[7:6] toggling at 1 Hz (right indicator)
All LEDs blink on over-temp warning
```

**Reverse gear + left indicator:**
```
┌─────────────────┐
│ SPD:008 GR:R    │
│ RPM:0800 T:038C │
└─────────────────┘

PORTB[1:0] toggling at 1 Hz (left indicator)
```

### CAN Frame Trace (logic analyser)

```
ID     DLC  Data            Decoded
──────────────────────────────────────────────────────
0x10   2    4B 00           Speed = 0x004B = 75 km/h
0x20   1    33              Gear  = '3'
0x30   2    D4 0E           RPM   = 0x0ED4 = 3796 RPM
0x40   2    43 00           Temp  = 0x0043 = 67 °C
0x50   1    01              Ind   = e_ind_left
0x10   2    4C 00           Speed = 76 km/h
0x20   1    33              Gear  = '3'
...
```

---

## Build

### Prerequisites

- MPLAB X IDE v6.x
- XC8 Compiler v2.x
- PICkit 3/4 or ICD 4 programmer

### Steps

1. Open each ECU folder as a separate MPLAB X project.
2. Set device to **PIC18F4580** in project properties.
3. Set XC8 optimisation level to **2** for production.
4. Build → Program each board individually.
5. Connect RB2 (TX) and RB3 (RX) of all three boards to the CAN bus with 120 Ω termination resistors at each end.

### Fuse / Config Bits

```
#pragma config OSC  = HS       // High-speed oscillator (20 MHz crystal)
#pragma config WDT  = OFF      // Watchdog disabled
#pragma config LVP  = OFF      // Low-voltage programming off
#pragma config MCLRE = ON      // MCLR pin enabled
```

---

## File Reference

### Shared across all ECUs

| File       | Content                                                         |
|------------|-----------------------------------------------------------------|
| `msg_id.h` | `SPEED_MSG_ID 0x10`, `GEAR_MSG_ID 0x20`, `RPM_MSG_ID 0x30`, `ENG_TEMP_MSG_ID 0x40`, `INDICATOR_MSG_ID 0x50` |
| `can.h`    | ECAN register macros, `init_can()`, `can_transmit()`, `can_receive()` prototypes |
| `can.c`    | ECAN Mode 0, 500 kbps bit timing, standard frame TX/RX. Bug fix: `can_receive()` no longer discards valid frames by zeroing `*len` unconditionally |
| `uart.h/.c`| 9600 baud async UART debug: `init_uart()`, `putch()`, `puts()`, `getch()` |
| `adc.h/.c` | 10-bit ADC: right-justified, Fosc/32 clock, `init_adc()`, `read_adc(ch)` |
| `digital_keypad.h/.c` | `LEVEL` and `STATE_CHANGE` (edge-detect) read modes on PORTC[3:0] |

### ECU3-specific

| File                   | Content                                                    |
|------------------------|------------------------------------------------------------|
| `clcd.h/.c`            | HD44780 8-bit parallel driver on PORTD, control on RC[2:0]; `init_clcd()`, `clcd_print()`, `clcd_putch()`, `clcd_write()` |
| `timer0.h/.c`          | `init_timer0()`: 8-bit mode, Fosc/4, no prescaler, preload=6, TMR0IE enabled |
| `isr.h/.c`             | `__interrupt() isr()`: Timer0 blink (500 ms toggle) + CAN RX flag clear |
| `message_handler.h/.c` | `process_canbus_data()`, `handle_speed_data()`, `handle_gear_data()`, `handle_rpm_data()`, `handle_engine_temp_data()`, `handle_indicator_data()`, `write_decimal()` |
