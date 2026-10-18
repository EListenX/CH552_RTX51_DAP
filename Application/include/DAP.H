/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        1. December 2017
 * $Revision:    V2.0.0
 *
 * Project:      CMSIS-DAP Include
 * Title:        DAP.h Definitions
 *
 *---------------------------------------------------------------------------*/

#ifndef __DAP_H__
#define __DAP_H__

#include "CH552.H"
#include "DAP_Config.H"
#include <stdio.h>
#include <string.h>
#include <intrins.h>

// DAP firmware version (optional helper define)
//#define DAP_FW_VER "2.0.0"

// DAP command identifiers
#define ID_DAP_Info                 0x00U  // Get DAP information
#define ID_DAP_HostStatus           0x01U  // Set or get host status
#define ID_DAP_Connect              0x02U  // Connect to target
#define ID_DAP_Disconnect           0x03U  // Disconnect from target
#define ID_DAP_TransferConfigure    0x04U  // Configure transfer parameters
#define ID_DAP_Transfer             0x05U  // Single register transfer
#define ID_DAP_TransferBlock        0x06U  // Block register transfer
#define ID_DAP_TransferAbort        0x07U  // Abort current transfer
#define ID_DAP_WriteABORT           0x08U  // Write DP ABORT register
#define ID_DAP_Delay                0x09U  // Insert delay
#define ID_DAP_ResetTarget          0x0AU  // Reset target device

#define ID_DAP_SWJ_Pins             0x10U  // Control SWJ pins
#define ID_DAP_SWJ_Clock            0x11U  // Configure SWJ clock
#define ID_DAP_SWJ_Sequence         0x12U  // Generate SWJ sequence
#define ID_DAP_SWD_Configure        0x13U  // Configure SWD mode
#define ID_DAP_JTAG_Sequence        0x14U  // Generate JTAG sequence
#define ID_DAP_JTAG_Configure       0x15U  // Configure JTAG chain
#define ID_DAP_JTAG_IDCODE          0x16U  // Read JTAG IDCODE
#define ID_DAP_SWO_Transport        0x17U  // Configure SWO transport
#define ID_DAP_SWO_Mode             0x18U  // Configure SWO mode
#define ID_DAP_SWO_Baudrate         0x19U  // Configure SWO baudrate
#define ID_DAP_SWO_Control          0x1AU  // Control SWO capture
#define ID_DAP_SWO_Status           0x1BU  // Read SWO status
#define ID_DAP_SWO_Data             0x1CU  // Read SWO data
#define ID_DAP_SWO_ExtendedStatus   0x1EU  // Read extended SWO status
#define ID_DAP_SWD_Sequence         0x1DU  // SWD I/O sequence

#define ID_DAP_QueueCommands        0x7EU  // Queue multiple commands
#define ID_DAP_ExecuteCommands      0x7FU  // Execute queued commands

// DAP vendor-specific command identifiers
#define ID_DAP_Vendor0              0x80U  // Vendor command 0
#define ID_DAP_Vendor1              0x81U  // Vendor command 1
#define ID_DAP_Vendor2              0x82U  // Vendor command 2
#define ID_DAP_Vendor3              0x83U  // Vendor command 3
#define ID_DAP_Vendor4              0x84U  // Vendor command 4
#define ID_DAP_Vendor5              0x85U  // Vendor command 5
#define ID_DAP_Vendor6              0x86U  // Vendor command 6
#define ID_DAP_Vendor7              0x87U  // Vendor command 7
#define ID_DAP_Vendor8              0x88U  // Vendor command 8
#define ID_DAP_Vendor9              0x89U  // Vendor command 9
#define ID_DAP_Vendor10             0x8AU  // Vendor command 10
#define ID_DAP_Vendor11             0x8BU  // Vendor command 11
#define ID_DAP_Vendor12             0x8CU  // Vendor command 12
#define ID_DAP_Vendor13             0x8DU  // Vendor command 13
#define ID_DAP_Vendor14             0x8EU  // Vendor command 14
#define ID_DAP_Vendor15             0x8FU  // Vendor command 15
#define ID_DAP_Vendor16             0x90U  // Vendor command 16
#define ID_DAP_Vendor17             0x91U  // Vendor command 17
#define ID_DAP_Vendor18             0x92U  // Vendor command 18
#define ID_DAP_Vendor19             0x93U  // Vendor command 19
#define ID_DAP_Vendor20             0x94U  // Vendor command 20
#define ID_DAP_Vendor21             0x95U  // Vendor command 21
#define ID_DAP_Vendor22             0x96U  // Vendor command 22
#define ID_DAP_Vendor23             0x97U  // Vendor command 23
#define ID_DAP_Vendor24             0x98U  // Vendor command 24
#define ID_DAP_Vendor25             0x99U  // Vendor command 25
#define ID_DAP_Vendor26             0x9AU  // Vendor command 26
#define ID_DAP_Vendor27             0x9BU  // Vendor command 27
#define ID_DAP_Vendor28             0x9CU  // Vendor command 28
#define ID_DAP_Vendor29             0x9DU  // Vendor command 29
#define ID_DAP_Vendor30             0x9EU  // Vendor command 30
#define ID_DAP_Vendor31             0x9FU  // Vendor command 31

// DAP extended vendor command range
#define ID_DAP_VendorExFirst        0xA0U  // First extended vendor command
#define ID_DAP_VendorExLast         0xFEU  // Last extended vendor command

#define ID_DAP_Invalid              0xFFU  // Invalid or unsupported command

// Generic DAP status codes
#define DAP_OK                      0x00U  // Operation successful
#define DAP_ERROR                   0xFFU  // Generic error

// DAP Info IDs (used with DAP_Info command)
#define DAP_ID_VENDOR               0x01U  // Adapter vendor string
#define DAP_ID_PRODUCT              0x02U  // Adapter product string
#define DAP_ID_SER_NUM              0x03U  // Serial number string
#define DAP_ID_FW_VER               0x04U  // Firmware version string
#define DAP_ID_DEVICE_VENDOR        0x05U  // Target device vendor string
#define DAP_ID_DEVICE_NAME          0x06U  // Target device name string
#define DAP_ID_PRODUCT_FW_VER       0x09U  // Product firmware version, distinct from protocol version
#define DAP_ID_CAPABILITIES         0xF0U  // Adapter capability bits
#define DAP_ID_TIMESTAMP_CLOCK      0xF1U  // Timestamp clock frequency
#define DAP_ID_UART_RX_BUFFER_SIZE  0xFBU
#define DAP_ID_UART_TX_BUFFER_SIZE  0xFCU
#define DAP_ID_SWO_BUFFER_SIZE      0xFDU  // SWO buffer size in bytes
#define DAP_ID_PACKET_COUNT         0xFEU  // Number of DAP packets
#define DAP_ID_PACKET_SIZE          0xFFU  // Size of each DAP packet

// DAP host / target status codes
#define DAP_DEBUGGER_CONNECTED      0x00U  // Debugger connection status
#define DAP_TARGET_RUNNING          0x01U  // Target execution status

// DAP port selection
#define DAP_PORT_AUTODETECT         0x00U  // Auto-detect port
#define DAP_PORT_DISABLED           0x00U  // Port disabled
#define DAP_PORT_SWD                0x01U  // SWD port
#define DAP_PORT_JTAG               0x02U  // JTAG port

// SWJ pin indices used in DAP_SWJ_Pins
#define DAP_SWJ_SWCLK_TCK           0x00U  // SWCLK/TCK index
#define DAP_SWJ_SWDIO_TMS           0x01U  // SWDIO/TMS index
#define DAP_SWJ_TDI                 0x02U  // TDI index
#define DAP_SWJ_TDO                 0x03U  // TDO index
#define DAP_SWJ_nTRST               0x05U  // nTRST index
#define DAP_SWJ_nRESET              0x07U  // nRESET index

// Bit masks for building SWJ pin state values
#define DAP_SWJ_SWCLK_TCK_BIT       0x01U  // SWCLK/TCK bit mask
#define DAP_SWJ_SWDIO_TMS_BIT       0x02U  // SWDIO/TMS bit mask
#define DAP_SWJ_TDI_BIT             0x04U  // TDI bit mask
#define DAP_SWJ_TDO_BIT             0x08U  // TDO bit mask
#define DAP_SWJ_nTRST_BIT           0x20U  // nTRST bit mask
#define DAP_SWJ_nRESET_BIT          0x80U  // nRESET bit mask

// DAP transfer request bit definitions
#define DAP_TRANSFER_APnDP          (1U << 0)  // Access Port / Debug Port select
#define DAP_TRANSFER_RnW            (1U << 1)  // Read (1) or write (0)
#define DAP_TRANSFER_A2             (1U << 2)  // Address bit A2
#define DAP_TRANSFER_A3             (1U << 3)  // Address bit A3
#define DAP_TRANSFER_MATCH_VALUE    (1U << 4)  // Enable value match
#define DAP_TRANSFER_MATCH_MASK     (1U << 5)  // Enable match mask
#define DAP_TRANSFER_TIMESTAMP      (1U << 7)  // Capture timestamp

// DAP transfer response bit definitions
#define DAP_TRANSFER_OK             (1U << 0)  // Transfer completed successfully
#define DAP_TRANSFER_WAIT           (1U << 1)  // Target not ready, retry
#define DAP_TRANSFER_FAULT          (1U << 2)  // Target reported a fault
#define DAP_TRANSFER_ERROR          (1U << 3)  // Protocol or parity error
#define DAP_TRANSFER_MISMATCH       (1U << 4)  // Match comparison failed

// DAP SWO trace encoding modes
#define DAP_SWO_OFF                 0x00U  // SWO disabled
#define DAP_SWO_UART                0x01U  // SWO UART/NRZ mode
#define DAP_SWO_MANCHESTER          0x02U  // SWO Manchester mode

// DAP SWO trace status bits
#define DAP_SWO_CAPTURE_ACTIVE      (1U << 0)  // SWO capture is active
#define DAP_SWO_CAPTURE_PAUSED      (1U << 1)  // SWO capture is paused
#define DAP_SWO_STREAM_ERROR        (1U << 6)  // Streaming error occurred
#define DAP_SWO_BUFFER_OVERRUN      (1U << 7)  // SWO buffer overrun

// Debug Port register addresses
#define DP_IDCODE                   0x00U  // IDCODE (JTAG read)
#define DP_ABORT                    0x00U  // Abort (JTAG write / SWD write)
#define DP_CTRL_STAT                0x04U  // Control / status register
#define DP_WCR                      0x04U  // Wire Control Register (SWD only)
#define DP_SELECT                   0x08U  // Select (JTAG R/W and SWD write)
#define DP_RESEND                   0x08U  // Resend (SWD read)
#define DP_RDBUFF                   0x0CU  // Read buffer (read-only)

// JTAG instruction register encodings
#define JTAG_ABORT                  0x08U  // Abort
#define JTAG_DPACC                  0x0AU  // DPACC
#define JTAG_APACC                  0x0BU  // APACC
#define JTAG_IDCODE                 0x0EU  // IDCODE
#define JTAG_BYPASS                 0x0FU  // BYPASS

// JTAG sequence information (JTAG_Sequence)
#define JTAG_SEQUENCE_TCK           0x3FU  // Number of TCK cycles
#define JTAG_SEQUENCE_TMS           0x40U  // TMS output value
#define JTAG_SEQUENCE_TDO           0x80U  // TDO capture enable

// SWD sequence information (SWD_Sequence)
#define SWD_SEQUENCE_CLK            0x3FU  // Number of SWCLK cycles
#define SWD_SEQUENCE_DIN            0x80U  // Capture SWDIO data


//#define DAP_PACKET_COUNT 2
//#define DAP_PACKET_SIZE 64 //THIS_ENDP0_SIZE
//#define DAP_DEFAULT_PORT DAP_PORT_SWD


// DAP main runtime data
typedef struct {
    UINT8     debug_port;        // Current debug port (SWD/JTAG/SWJ)
    UINT8     fast_clock;        // Fast-clock mode flag
    UINT16    clock_delay;       // SWJ clock delay cycles
    UINT8     nominal_clock[4];  // Limited SWJ clock in Hz, little-endian bytes
    UINT16    timestamp;         // Last captured timestamp

    struct {                     // Transfer configuration (generic)
        UINT8   idle_cycles;     // Idle cycles after transfer
        UINT8   retry_count_l;   // WAIT retry count low byte
        UINT8   retry_count_h;   // WAIT retry count high byte
        UINT8   match_retry_l;   // Match retry count low byte
        UINT8   match_retry_h;   // Match retry count high byte
#if (ENABLE_DAP_MATCH != 0)
        UINT8   match_mask[4];   // Value compare mask when match is enabled
#endif
    } transfer;

    struct { // SWD and JTAG settings survive port switching independently.
#if (DAP_SWD != 0)
        struct {                 // SWD-specific configuration
            UINT8 turnaround;    // Turnaround clock cycles
            UINT8 data_phase;    // Enable data phase (1) or not (0)
        } swd;
#endif
#if (DAP_JTAG != 0)
        struct {                 // JTAG-specific configuration
            UINT8  count;        // Number of devices in scan chain
            UINT8  index;        // Index of current device
#if (DAP_JTAG_DEV_CNT != 0)
            UINT8  ir_length[DAP_JTAG_DEV_CNT]; // IR length of each device
            // Additional IR before/after fields are omitted to save RAM:
            // UINT16 ir_before[DAP_JTAG_DEV_CNT];
            // UINT16 ir_after [DAP_JTAG_DEV_CNT];
#endif
        } jtag;
#endif
    } mode_cfg; // Mode-specific configuration (SWD/JTAG)
} DAP_Data_t;


// DAP state flags for USB ping‑pong channels (length = 2)
extern volatile UINT8 idata DapState[2]; // Per-channel flags and OUT/IN channel selectors
extern volatile UINT8 idata DAP_RequestLength[2]; // Actual USB OUT size per buffer
extern volatile UINT8 idata DAP_UsbEpoch; // Invalidate in-flight reply after USB bus reset
extern xdata DAP_Data_t DAP_Data; // Global DAP configuration/state
extern volatile UINT8 idata DAP_TransferAbort_Flag; // USB ISR -> active transfer loop

// Bit positions inside DapState elements
#define DAP_CH_BIT      0   // Bit 0: channel index
#define DAP_OUT_BIT     1   // Bit 1: OUT busy / ready flag
#define DAP_IN_BIT      2   // Bit 2: IN busy / ready flag

// Convenience access macros for DAP IN/OUT channel and state
#define DAP_GET_OUT_CH()   (((DapState[0]) >> DAP_CH_BIT) & 0x01)
#define DAP_SET_OUT_CH(b)  (DapState[0] = (DapState[0] & ~(1<<DAP_CH_BIT)) | (((b)&1) << DAP_CH_BIT))

#define DAP_GET_IN_CH()    (((DapState[1]) >> DAP_CH_BIT) & 0x01)
#define DAP_SET_IN_CH(b)   (DapState[1] = (DapState[1] & ~(1<<DAP_CH_BIT)) | (((b)&1) << DAP_CH_BIT))

#define DAP_GET_OUT_STATE(a)     (((DapState[a]) >> DAP_OUT_BIT) & 0x01)
#define DAP_SET_OUT_STATE(a, b)  (DapState[a] = (DapState[a] & ~(1<<DAP_OUT_BIT)) | (((b)&1) << DAP_OUT_BIT))

#define DAP_GET_IN_STATE(a)      (((DapState[a]) >> DAP_IN_BIT) & 0x01)
#define DAP_SET_IN_STATE(a, b)   (DapState[a] = (DapState[a] & ~(1<<DAP_IN_BIT)) | (((b)&1) << DAP_IN_BIT))




//extern BOOL DAP_LED_BUSY;


//extern void DAP_Thread(void);
// 引脚初始化/释放使用 IO_Config.H 的宏，不提供历史空壳函数。
#if DAP_SWD_SMALL_CORE
// 仅单次传输使用 small 自动变量；专用 IDATA 数据指针和全工程 LARGE 保持不变。
#define DAP_SWD_MEMORY small
#else
#define DAP_SWD_MEMORY
#endif
extern UINT8I SWD_Transfer(UINT8 reqI, UINT8I *datas) DAP_SWD_MEMORY;
extern void SWJ_Sequence(UINT16I count, const UINT8 xdata *datas);
extern void SWD_Sequence(UINT8I info, const UINT8 xdata *swdo, UINT8 xdata *swdi);
#if DAP_JTAG
#if DAP_SPI_JTAG || DAP_JTAG_GPIO_CORE
#define DAP_JTAG_MEMORY small
#else
#define DAP_JTAG_MEMORY
#endif
extern void JTAG_Sequence(UINT8I info, const UINT8 xdata *tdi, UINT8 xdata *tdo) DAP_JTAG_MEMORY;
extern void JTAG_InvalidateIR(void);
extern UINT8I JTAG_Transfer(UINT8I request_value, UINT8I *datas) DAP_JTAG_MEMORY;
extern void JTAG_ReadIDCode(UINT8I *datas) DAP_JTAG_MEMORY;
extern void JTAG_WriteAbort(UINT8I *datas) DAP_JTAG_MEMORY;
#endif


// Fixed delay for fast clock generation
#ifndef DELAY_FAST_CYCLES
#define DELAY_FAST_CYCLES 0U // Number of cycles: 0..3
#endif
#if (DELAY_FAST_CYCLES == 0U)
#define PIN_DELAY_FAST()    ;
#endif
#if (DELAY_FAST_CYCLES == 1U)
#define PIN_DELAY_FAST() NOP()
#endif
#if (DELAY_FAST_CYCLES == 2U)
#define PIN_DELAY_FAST() \
    NOP();               \
    NOP()
#endif
#if (DELAY_FAST_CYCLES == 3U)
#define PIN_DELAY_FAST() \
    NOP();               \
    NOP();               \
    NOP()
#endif

#endif /* __DAP_H__ */
