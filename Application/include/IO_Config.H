/**
 * @file    IO_Config.H
 * @brief   Hardware I/O Pin and Port abstraction for CH552 CMSIS-DAP
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __IO_CONFIG_H__
#define __IO_CONFIG_H__

#include "CH552.H"
#include "Board_Config.H"
#include "DAP_Config.H"
#if DAP_SPI_SWD
#include "DAP_SPI.H"
#define BOARD_SWD_SPI_SETUP() DAP_SPI_Init()
#else
#define BOARD_SWD_SPI_SETUP() do { } while (0)
#endif
#if DAP_SPI_SWD_SWJ
/* No byte transfer survives a command boundary; clear FIFO and release OE. */
#define BOARD_SWJ_SPI_OFF() do { SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL; SPI0_CTRL = bS0_MST_CLK; } while (0)
#else
#define BOARD_SWJ_SPI_OFF() do { } while (0)
#endif
#if DAP_SPI_JTAG
#include "DAP_SPI.H"
#define BOARD_JTAG_SPI_SETUP() DAP_SPI_Init()
#else
#define BOARD_JTAG_SPI_SETUP() do { } while (0)
#endif

/* Reject active pin collisions before emitting any GPIO operations. */
#define BOARD_PIN_ID(port, pin) ((port) * 8 + (pin))
#if SWO_UART && ((DAP_HAS_NRESET && PIN_nRESET_PORT == 3 && PIN_nRESET_BIT == 4) || BOARD_HAS_LED || (PIN_SWCLK_PORT == 3 && PIN_SWCLK_BIT == 4) || (PIN_SWDIO_PORT == 3 && PIN_SWDIO_BIT == 4))
#error SWO RXD1 P3.4 conflicts with reset, LED or debug pins
#endif
#if SWO_UART && DAP_JTAG
#if (PIN_TDI_PORT == 3 && PIN_TDI_BIT == 4) || (PIN_TDO_PORT == 3 && PIN_TDO_BIT == 4) || (DAP_HAS_NTRST && PIN_nTRST_PORT == 3 && PIN_nTRST_BIT == 4)
#error SWO RXD1 P3.4 conflicts with JTAG pins
#endif
#endif
#if DAP_SPI_SWD_2WIRE
#if PIN_SWCLK_PORT != 1 || PIN_SWCLK_BIT != 7 || PIN_SWDIO_PORT != 1 || PIN_SWDIO_BIT != 6
#error Two-wire SWD SPI requires SWCLK=P1.7 and SWDIO=P1.6
#endif
#elif DAP_SPI_SWD && (PIN_SWCLK_PORT != 1 || PIN_SWCLK_BIT != 7 || PIN_SWDIO_PORT != 1 || PIN_SWDIO_BIT != 5)
#error SWD SPI requires SWCLK=P1.7 and SWDIO=P1.5
#endif
#if DAP_SPI_JTAG && (PIN_SWCLK_PORT != 1 || PIN_SWCLK_BIT != 7 || PIN_TDI_PORT != 1 || PIN_TDI_BIT != 5 || PIN_TDO_PORT != 1 || PIN_TDO_BIT != 6)
#error JTAG SPI requires TCK=P1.7 TDI=P1.5 TDO=P1.6
#endif
#if DAP_JTAG
#define BOARD_TDI_ID BOARD_PIN_ID(PIN_TDI_PORT, PIN_TDI_BIT)
#define BOARD_TDO_ID BOARD_PIN_ID(PIN_TDO_PORT, PIN_TDO_BIT)
#define BOARD_TRST_ID BOARD_PIN_ID(PIN_nTRST_PORT, PIN_nTRST_BIT)
#define BOARD_JTAG_GPIO_VALID(port, pin) (((port) == 1 || (port) == 3) && (pin) >= 0 && (pin) <= 7)
#define BOARD_JTAG_RESERVED(id) ((id) == BOARD_PIN_ID(PIN_SWCLK_PORT, PIN_SWCLK_BIT) || (id) == BOARD_PIN_ID(PIN_SWDIO_PORT, PIN_SWDIO_BIT) || (id) == 24 || (id) == 25 || (DAP_HAS_NRESET && (id) == BOARD_PIN_ID(PIN_nRESET_PORT, PIN_nRESET_BIT)))
#if !BOARD_JTAG_GPIO_VALID(PIN_TDI_PORT, PIN_TDI_BIT) || !BOARD_JTAG_GPIO_VALID(PIN_TDO_PORT, PIN_TDO_BIT) || (DAP_HAS_NTRST && !BOARD_JTAG_GPIO_VALID(PIN_nTRST_PORT, PIN_nTRST_BIT))
#error Invalid JTAG GPIO port or bit
#endif
#if BOARD_TDI_ID == BOARD_TDO_ID || BOARD_JTAG_RESERVED(BOARD_TDI_ID) || BOARD_JTAG_RESERVED(BOARD_TDO_ID)
#error JTAG pin conflicts with debug or CDC UART pins
#endif
#if DAP_HAS_NTRST && (BOARD_TRST_ID == BOARD_TDI_ID || BOARD_TRST_ID == BOARD_TDO_ID || BOARD_JTAG_RESERVED(BOARD_TRST_ID))
#error nTRST pin conflicts with debug or CDC UART pins
#endif
#if BOARD_HAS_LED
#if BOARD_PIN_ID(PIN_LED_CONNECTED_PORT, PIN_LED_CONNECTED_BIT) == BOARD_TDI_ID || BOARD_PIN_ID(PIN_LED_CONNECTED_PORT, PIN_LED_CONNECTED_BIT) == BOARD_TDO_ID || BOARD_PIN_ID(PIN_LED_RUNNING_PORT, PIN_LED_RUNNING_BIT) == BOARD_TDI_ID || BOARD_PIN_ID(PIN_LED_RUNNING_PORT, PIN_LED_RUNNING_BIT) == BOARD_TDO_ID || (DAP_HAS_NTRST && (BOARD_PIN_ID(PIN_LED_CONNECTED_PORT, PIN_LED_CONNECTED_BIT) == BOARD_TRST_ID || BOARD_PIN_ID(PIN_LED_RUNNING_PORT, PIN_LED_RUNNING_BIT) == BOARD_TRST_ID))
#error LED pin conflicts with JTAG pins
#endif
#endif
#endif
#if DAP_HAS_NRESET && ((BOARD_PIN_ID(PIN_nRESET_PORT, PIN_nRESET_BIT) == BOARD_PIN_ID(PIN_SWCLK_PORT, PIN_SWCLK_BIT)) || (BOARD_PIN_ID(PIN_nRESET_PORT, PIN_nRESET_BIT) == BOARD_PIN_ID(PIN_SWDIO_PORT, PIN_SWDIO_BIT)))
#error nRESET conflicts with an active SWD pin
#endif
#if BOARD_HAS_LED
#if ((PIN_LED_CONNECTED_PORT != 1) && (PIN_LED_CONNECTED_PORT != 3)) || ((PIN_LED_RUNNING_PORT != 1) && (PIN_LED_RUNNING_PORT != 3)) || PIN_LED_CONNECTED_BIT < 0 || PIN_LED_CONNECTED_BIT > 7 || PIN_LED_RUNNING_BIT < 0 || PIN_LED_RUNNING_BIT > 7
#error Invalid LED GPIO port or bit
#endif
#define BOARD_LED_CON_ID BOARD_PIN_ID(PIN_LED_CONNECTED_PORT, PIN_LED_CONNECTED_BIT)
#define BOARD_LED_RUN_ID BOARD_PIN_ID(PIN_LED_RUNNING_PORT, PIN_LED_RUNNING_BIT)
#define BOARD_SWCLK_ID BOARD_PIN_ID(PIN_SWCLK_PORT, PIN_SWCLK_BIT)
#define BOARD_SWDIO_ID BOARD_PIN_ID(PIN_SWDIO_PORT, PIN_SWDIO_BIT)
#if BOARD_LED_CON_ID == BOARD_LED_RUN_ID || BOARD_LED_CON_ID == BOARD_SWCLK_ID || BOARD_LED_CON_ID == BOARD_SWDIO_ID || BOARD_LED_RUN_ID == BOARD_SWCLK_ID || BOARD_LED_RUN_ID == BOARD_SWDIO_ID
#error LED pins conflict with each other or SWD
#endif
#if DAP_HAS_NRESET && ((BOARD_LED_CON_ID == BOARD_PIN_ID(PIN_nRESET_PORT, PIN_nRESET_BIT)) || (BOARD_LED_RUN_ID == BOARD_PIN_ID(PIN_nRESET_PORT, PIN_nRESET_BIT)))
#error LED pin conflicts with nRESET; remap the LED or disable it
#endif
#endif

/**
 * CH552 GPIO mode helper (per-bit configuration):
 *
 * Push-Pull output:
 *   port##_MOD_OC &= ~(1 << n);
 *   port##_DIR_PU |=  (1 << n);
 *
 * High-Impedance input:
 *   port##_MOD_OC &= ~(1 << n);
 *   port##_DIR_PU &=  ~(1 << n);
 *
 * Open-Drain output:
 *   port##_MOD_OC |=  (1 << n);
 *   port##_DIR_PU |=  (1 << n);
 *
 * Quasi-Bidirectional:
 *   port##_MOD_OC |=  (1 << n);
 *   port##_DIR_PU |=  (1 << n);
 */

// ============================================================================
// Configure DAP I/O pins
// ============================================================================

/* JTAG drives TCK/TMS/TDI, never TDO. nTRST is optional, no guessed wiring. */
#if DAP_JTAG
#define PORT_JTAG_SETUP() \
    do { \
        BOARD_SWJ_SPI_OFF(); \
        BOARD_JTAG_SPI_SETUP(); \
        PIN_SWCLK = 1; PIN_SWDIO = 1; PIN_TDI = 1; PIN_TDO = 1; \
        BOARD_SWD_DIR_SETUP(); \
        BOARD_GPIO_OC(PIN_SWCLK_PORT) &= ~PIN_SWCLK_MASK; \
        BOARD_GPIO_PU(PIN_SWCLK_PORT) |= PIN_SWCLK_MASK; \
        BOARD_SWD_DIR_OUTPUT(); \
        BOARD_GPIO_OC(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
        BOARD_GPIO_PU(PIN_SWDIO_PORT) |= PIN_SWDIO_MASK; \
        BOARD_GPIO_OC(PIN_TDI_PORT) &= ~PIN_TDI_MASK; \
        BOARD_GPIO_PU(PIN_TDI_PORT) |= PIN_TDI_MASK; \
        BOARD_GPIO_OC(PIN_TDO_PORT) &= ~PIN_TDO_MASK; \
        BOARD_GPIO_PU(PIN_TDO_PORT) &= ~PIN_TDO_MASK; \
        JTAG_RESET_SETUP(); \
    } while (0)
#define JTAG_PINS_OFF() \
    do { \
        BOARD_GPIO_OC(PIN_TDI_PORT) &= ~PIN_TDI_MASK; \
        BOARD_GPIO_PU(PIN_TDI_PORT) &= ~PIN_TDI_MASK; \
        PIN_TDI = 1; JTAG_RESET_OFF(); \
    } while (0)
#else
#define PORT_JTAG_SETUP() do { } while (0)
#define JTAG_PINS_OFF() do { } while (0)
#endif

/** Setup SWD I/O pins: SWCLK, SWDIO, and nRESET.
 * Configures the DAP Hardware I/O pins for Serial Wire Debug (SWD) mode:
 *  - SWCLK to push-pull output
 *  - SWDIO to open-drain / quasi-bidirectional with pull-up
 *  - Optional nRESET is configured separately when DAP_HAS_NRESET != 0
 */
#if BOARD_SWD_INTERFACE_DIRECT
#define BOARD_SWD_BUFFER_RELEASE() do { } while (0)
#else
#define BOARD_SWD_BUFFER_RELEASE() \
    do { \
        BOARD_GPIO_OC(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
        BOARD_GPIO_PU(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
    } while (0)
#endif
#define PORT_SWD_SETUP() \
    do { \
        BOARD_SWD_SPI_SETUP(); \
        BOARD_SWD_BUFFER_RELEASE(); \
        BOARD_SWD_DIR_SETUP(); \
        PIN_SWCLK = 1; \
        PIN_SWDIO = 1; \
        BOARD_GPIO_OC(PIN_SWCLK_PORT) &= ~PIN_SWCLK_MASK; \
        BOARD_GPIO_PU(PIN_SWCLK_PORT) |= PIN_SWCLK_MASK; \
        PIN_SWDIO_OUT_ENABLE(); \
    } while (0)

/** Disable JTAG/SWD I/O Pins.
 * Configures SWCLK and SWDIO to High-Z mode.
 */
#if PIN_SWCLK_PORT == PIN_SWDIO_PORT
#define BOARD_SWD_GPIO_OFF() \
    do { \
        BOARD_GPIO_OC(PIN_SWCLK_PORT) &= ~(PIN_SWCLK_MASK | PIN_SWDIO_MASK); \
        BOARD_GPIO_PU(PIN_SWCLK_PORT) &= ~(PIN_SWCLK_MASK | PIN_SWDIO_MASK); \
    } while (0)
#else
#define BOARD_SWD_GPIO_OFF() \
    do { \
        BOARD_GPIO_OC(PIN_SWCLK_PORT) &= ~PIN_SWCLK_MASK; \
        BOARD_GPIO_PU(PIN_SWCLK_PORT) &= ~PIN_SWCLK_MASK; \
        BOARD_GPIO_OC(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
        BOARD_GPIO_PU(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
    } while (0)
#endif
#define PORT_OFF() \
    do { \
        BOARD_SWJ_SPI_OFF(); \
        BOARD_SWD_GPIO_OFF(); \
        BOARD_SWD_DIR_OFF(); \
        PIN_SWCLK = 1; \
        PIN_SWDIO = 1; \
        RESET_PIN_OFF(); \
        JTAG_PINS_OFF(); \
    } while (0)

// ============================================================================
// SWCLK/TCK I/O pin
// ============================================================================

/** SWCLK/TCK I/O pin: Get Input */
#define PIN_SWCLK_TCK_IN()      (PIN_SWCLK & 0x01)

/** SWCLK/TCK I/O pin: Set Output to High */
#define PIN_SWCLK_TCK_SET()     PIN_SWCLK = 1

/** SWCLK/TCK I/O pin: Set Output to Low */
#define PIN_SWCLK_TCK_CLR()     PIN_SWCLK = 0

// ============================================================================
// SWDIO/TMS Pin I/O
// ============================================================================

/** SWDIO/TMS I/O pin: Get Input */
#define PIN_SWDIO_TMS_IN()      (PIN_SWDIO & 0x01)

/** SWDIO/TMS I/O pin: Set Output to High */
#define PIN_SWDIO_TMS_SET()     PIN_SWDIO = 1

/** SWDIO/TMS I/O pin: Set Output to Low */
#define PIN_SWDIO_TMS_CLR()     PIN_SWDIO = 0

/** SWDIO I/O pin: Get Input (used in SWD mode only) */
#define PIN_SWDIO_IN()          (PIN_SWDIO & 0x01)

/** SWDIO I/O pin: Set Output (used in SWD mode only) */
#define PIN_SWDIO_OUT(bit)      (PIN_SWDIO = ((bit) & 0x01))

/** SWDIO I/O pin: Switch to Output mode (used in SWD mode only) */
#if BOARD_SWD_INTERFACE_DIRECT
#define BOARD_SWDIO_DRIVE_MODE() do { BOARD_GPIO_OC(PIN_SWDIO_PORT) |= PIN_SWDIO_MASK; } while (0)
#else
/* Buffered board: drive the MCU side push-pull only after input turnaround. */
#define BOARD_SWDIO_DRIVE_MODE() do { BOARD_GPIO_OC(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; } while (0)
#endif
#define PIN_SWDIO_OUT_ENABLE() \
    do { \
        BOARD_SWD_DIR_OUTPUT(); \
        BOARD_SWDIO_DRIVE_MODE(); \
        BOARD_GPIO_PU(PIN_SWDIO_PORT) |= PIN_SWDIO_MASK; \
    } while (0)

/** SWDIO I/O pin: Switch to Input mode (used in SWD mode only) */
#define PIN_SWDIO_OUT_DISABLE() \
    do { \
        PIN_SWDIO = 1; \
        BOARD_GPIO_OC(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
        BOARD_GPIO_PU(PIN_SWDIO_PORT) &= ~PIN_SWDIO_MASK; \
        BOARD_SWD_DIR_INPUT(); \
    } while (0)

// ============================================================================
// JTAG Pin I/O and optional TAP reset
// ============================================================================

#if DAP_JTAG
#define PIN_TDI_IN()            (PIN_TDI & 1U)
#define PIN_TDI_OUT(bit)        (PIN_TDI = ((bit) & 1U))
#if DAP_JTAG_BIT_CLOCK
#define PIN_TDO_IN()            (PIN_TDO) // Keep C51 bit type for the carry-return backend.
#else
#define PIN_TDO_IN()            (PIN_TDO & 1U)
#endif
#else
#define PIN_TDI_IN()            (0)
#define PIN_TDI_OUT(bit)
#define PIN_TDO_IN()            (0)
#endif
#if DAP_JTAG && DAP_HAS_NTRST
#define PIN_nTRST_IN()          (PIN_nTRST & 1U)
#define PIN_nTRST_OUT(bit)      (PIN_nTRST = ((bit) & 1U))
#define JTAG_RESET_SETUP() do { PIN_nTRST = 1; BOARD_GPIO_OC(PIN_nTRST_PORT) |= PIN_nTRST_MASK; BOARD_GPIO_PU(PIN_nTRST_PORT) |= PIN_nTRST_MASK; } while (0)
#define JTAG_RESET_OFF() do { BOARD_GPIO_OC(PIN_nTRST_PORT) &= ~PIN_nTRST_MASK; BOARD_GPIO_PU(PIN_nTRST_PORT) &= ~PIN_nTRST_MASK; PIN_nTRST = 1; } while (0)
#else
#define PIN_nTRST_IN()          (1U)
#define PIN_nTRST_OUT(bit)      do { } while (0)
#define JTAG_RESET_SETUP()      do { } while (0)
#define JTAG_RESET_OFF()        do { } while (0)
#endif

// ============================================================================
// nRESET Pin I/O
// ============================================================================

#if (DAP_HAS_NRESET != 0)
#define PIN_nRESET_IN()         (PIN_nRESET & 1U)
#define RESET_PIN_SETUP() \
    do { PIN_nRESET = 1; BOARD_GPIO_OC(PIN_nRESET_PORT) |= PIN_nRESET_MASK; BOARD_GPIO_PU(PIN_nRESET_PORT) |= PIN_nRESET_MASK; } while (0)
#define RESET_PIN_OFF() \
    do { BOARD_GPIO_OC(PIN_nRESET_PORT) &= ~PIN_nRESET_MASK; BOARD_GPIO_PU(PIN_nRESET_PORT) &= ~PIN_nRESET_MASK; PIN_nRESET = 1; } while (0)
#define PIN_nRESET_OUT(bit) \
    do { RESET_PIN_SETUP(); PIN_nRESET = ((bit) & 1U); } while (0)
#else
#define PIN_nRESET_IN()         (1U)
#define RESET_PIN_SETUP()       do { } while (0)
#define RESET_PIN_OFF()         do { } while (0)
#define PIN_nRESET_OUT(bit)     do { } while (0)
#endif

// ============================================================================
// Status LEDs & Timestamp
// ============================================================================

#if (BOARD_HAS_LED != 0)
#define LED_CONNECTED_OUT(bit)  do { PIN_LED_CONNECTED = ((bit) ? 0 : 1); BOARD_GPIO_OC(PIN_LED_CONNECTED_PORT) &= ~(1U << PIN_LED_CONNECTED_BIT); BOARD_GPIO_PU(PIN_LED_CONNECTED_PORT) |= (1U << PIN_LED_CONNECTED_BIT); } while (0)
#define LED_RUNNING_OUT(bit)    do { PIN_LED_RUNNING = ((bit) ? 0 : 1); BOARD_GPIO_OC(PIN_LED_RUNNING_PORT) &= ~(1U << PIN_LED_RUNNING_BIT); BOARD_GPIO_PU(PIN_LED_RUNNING_PORT) |= (1U << PIN_LED_RUNNING_BIT); } while (0)
#else
#define LED_CONNECTED_OUT(bit)
#define LED_RUNNING_OUT(bit)
#endif

// Timestamp: not implemented, returns 0
#define TIMESTAMP_GET()         (0U)

#endif /* __IO_CONFIG_H__ */
