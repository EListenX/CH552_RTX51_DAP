/**
 * @file    Board_Config.H
 * @brief   Target board hardware configuration for CH552 CMSIS-DAP
 *
 * Defines board identification, system clock frequency, debug port topology,
 * and physical GPIO pin mapping. Serves as the single source of truth for
 * target board hardware characteristics (inspired by ARM DAPLink board layer).
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "CH552.H"
#include "CH55x_Config.H"
#include "Board_Presets.H"

/*-----------------------------------------------------------------------------
 * 1. Target Board Identification
 *----------------------------------------------------------------------------*/
#define BOARD_NAME              "CH552 CMSIS-DAP"
#define BOARD_VENDOR            "WhyCan"
#define BOARD_HARDWARE_VERSION  "V1.0"

/*-----------------------------------------------------------------------------
 * 2. System Clock Configuration
 *----------------------------------------------------------------------------*/
/* SYSTEM_CLOCK_HZ is shared with Library/Debug.H via CH55x_Config.H. */

/*-----------------------------------------------------------------------------
 * 3. Debug Port Topology & Configuration
 *----------------------------------------------------------------------------*/
/* SWD interface connection: 1 = Direct MCU pin connection, 0 = External buffer */
#ifndef BOARD_SWD_INTERFACE_DIRECT
#define BOARD_SWD_INTERFACE_DIRECT  1
#endif

/* Direction control pin support: 0 = None (direct), 1 = Reserved for buffer DIR */
#ifndef BOARD_DIR_CONTROL_SUPPORT
#define BOARD_DIR_CONTROL_SUPPORT   0
#endif

/* Status LED support: 0 = Disabled, 1 = Enabled */
#ifndef BOARD_HAS_LED
#define BOARD_HAS_LED               0
#endif

/* No guessed DIR pin or polarity: a buffered board supplies these four hooks.
 * SETUP leaves the transceiver in input direction; OFF releases/disables it. */
#if BOARD_SWD_INTERFACE_DIRECT == 1
#if BOARD_DIR_CONTROL_SUPPORT != 0
#error Direct SWD must not enable external DIR control
#endif
#define BOARD_SWD_DIR_SETUP() do { } while (0)
#define BOARD_SWD_DIR_INPUT() do { } while (0)
#define BOARD_SWD_DIR_OUTPUT() do { } while (0)
#define BOARD_SWD_DIR_OFF() do { } while (0)
#elif BOARD_SWD_INTERFACE_DIRECT == 0
#if BOARD_DIR_CONTROL_SUPPORT != 1
#error Buffered SWD requires DIR control
#endif
#if !defined(BOARD_SWD_DIR_SETUP) || !defined(BOARD_SWD_DIR_INPUT) || !defined(BOARD_SWD_DIR_OUTPUT) || !defined(BOARD_SWD_DIR_OFF)
#error Define all BOARD_SWD_DIR hooks for the actual buffer pin and polarity
#endif
#else
#error BOARD_SWD_INTERFACE_DIRECT must be 0 or 1
#endif
#if BOARD_HAS_LED != 0 && BOARD_HAS_LED != 1
#error BOARD_HAS_LED must be 0 or 1
#endif

/*-----------------------------------------------------------------------------
 * 4. Physical Pin Assignments (8051 sbit & bit masks)
 *----------------------------------------------------------------------------*/

/* --- SWD / JTAG Clock Pin (P1.5) --- */
#define BOARD_GPIO_PORT_(n)      P##n
#define BOARD_GPIO_PORT(n)       BOARD_GPIO_PORT_(n)
#define BOARD_GPIO_OC_(n)        P##n##_MOD_OC
#define BOARD_GPIO_OC(n)         BOARD_GPIO_OC_(n)
#define BOARD_GPIO_PU_(n)        P##n##_DIR_PU
#define BOARD_GPIO_PU(n)         BOARD_GPIO_PU_(n)

#ifndef PIN_SWCLK_PORT
#define PIN_SWCLK_PORT          1
#endif
#ifndef PIN_SWCLK_BIT
#define PIN_SWCLK_BIT           5
#endif
sbit PIN_SWCLK = BOARD_GPIO_PORT(PIN_SWCLK_PORT) ^ PIN_SWCLK_BIT;
#define PIN_SWCLK_MASK          (1U << PIN_SWCLK_BIT)

/* --- SWD / JTAG Data Pin (P1.7) --- */
#ifndef PIN_SWDIO_PORT
#define PIN_SWDIO_PORT          1
#endif
#ifndef PIN_SWDIO_BIT
#define PIN_SWDIO_BIT           7
#endif
sbit PIN_SWDIO = BOARD_GPIO_PORT(PIN_SWDIO_PORT) ^ PIN_SWDIO_BIT;
#define PIN_SWDIO_MASK          (1U << PIN_SWDIO_BIT)

/* --- JTAG dedicated pins; remapping remains a board configuration choice. --- */
#define PIN_TCK                 PIN_SWCLK
#define PIN_TMS                 PIN_SWDIO

#ifndef PIN_TDI_PORT
#define PIN_TDI_PORT            1
#endif
#ifndef PIN_TDI_BIT
#define PIN_TDI_BIT             6
#endif
sbit PIN_TDI = BOARD_GPIO_PORT(PIN_TDI_PORT) ^ PIN_TDI_BIT;
#define PIN_TDI_MASK            (1U << PIN_TDI_BIT)

#ifndef PIN_TDO_PORT
#define PIN_TDO_PORT            3
#endif
#ifndef PIN_TDO_BIT
#define PIN_TDO_BIT             3       /* Diagnostic remap; nTRST must stay disabled or move. */
#endif
sbit PIN_TDO = BOARD_GPIO_PORT(PIN_TDO_PORT) ^ PIN_TDO_BIT;
#define PIN_TDO_MASK            (1U << PIN_TDO_BIT)

#ifndef PIN_nTRST_PORT
#define PIN_nTRST_PORT          3
#endif
#ifndef PIN_nTRST_BIT
#define PIN_nTRST_BIT           3
#endif
sbit PIN_nTRST = BOARD_GPIO_PORT(PIN_nTRST_PORT) ^ PIN_nTRST_BIT;
#define PIN_nTRST_MASK          (1U << PIN_nTRST_BIT)

/* SWO uses UART1's alternate RX pin. TXD1=P3.2 remains unused/high-Z.
 * Do not use native UART1 mapping: its TXD1=P1.7 overlaps the debug pins.
 */
#define PIN_SWO_PORT            3
#define PIN_SWO_BIT             4
#define PIN_SWO_MASK            0x10U

/* --- Target Hardware Reset Pin (P3.4) --- */
#ifndef PIN_nRESET_PORT
#define PIN_nRESET_PORT         3
#endif
#ifndef PIN_nRESET_BIT
#define PIN_nRESET_BIT          4
#endif
sbit PIN_nRESET = BOARD_GPIO_PORT(PIN_nRESET_PORT) ^ PIN_nRESET_BIT;
#define PIN_nRESET_MASK         (1U << PIN_nRESET_BIT)

/* --- Optional Status LEDs --- */
#if (BOARD_HAS_LED != 0)
#ifndef PIN_LED_CONNECTED_PORT
#define PIN_LED_CONNECTED_PORT 3
#endif
#ifndef PIN_LED_CONNECTED_BIT
#define PIN_LED_CONNECTED_BIT 4
#endif
#ifndef PIN_LED_RUNNING_PORT
#define PIN_LED_RUNNING_PORT 3
#endif
#ifndef PIN_LED_RUNNING_BIT
#define PIN_LED_RUNNING_BIT 5
#endif
sbit PIN_LED_CONNECTED = BOARD_GPIO_PORT(PIN_LED_CONNECTED_PORT) ^ PIN_LED_CONNECTED_BIT;
sbit PIN_LED_RUNNING = BOARD_GPIO_PORT(PIN_LED_RUNNING_PORT) ^ PIN_LED_RUNNING_BIT;
#endif

#if ((PIN_SWCLK_PORT != 1) && (PIN_SWCLK_PORT != 3)) || ((PIN_SWDIO_PORT != 1) && (PIN_SWDIO_PORT != 3)) || ((PIN_nRESET_PORT != 1) && (PIN_nRESET_PORT != 3))
#error Debug pins must use supported P1 or P3 GPIO ports
#endif
#if PIN_SWCLK_BIT < 0 || PIN_SWCLK_BIT > 7 || PIN_SWDIO_BIT < 0 || PIN_SWDIO_BIT > 7 || PIN_nRESET_BIT < 0 || PIN_nRESET_BIT > 7
#error Debug pin bit must be in 0..7
#endif
#if PIN_SWCLK_PORT == PIN_SWDIO_PORT && PIN_SWCLK_BIT == PIN_SWDIO_BIT
#error SWCLK and SWDIO must not share a pin
#endif

/*-----------------------------------------------------------------------------
 * 5. Legacy Pin Name Aliases (Backward Compatibility)
 *----------------------------------------------------------------------------*/
#define SWD_CLK_IO              PIN_SWCLK
#define SWD_DAT_IO              PIN_SWDIO
#define JTAG_TCK_IO             PIN_SWCLK
#define JTAG_TMS_IO             PIN_SWDIO
#define JTAG_TDI_IO             PIN_TDI
#define JTAG_TDO_IO             PIN_TDO
#define JTAG_nTRST_IO           PIN_nTRST
#define JTAG_nRESET_IO          PIN_nRESET

#endif /* __BOARD_CONFIG_H__ */
