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
 * Project:      CMSIS-DAP Source
 * Title:        DAP.c CMSIS-DAP Commands
 *
 *---------------------------------------------------------------------------*/

#include "DAP.h"
#include "DAP_Config.H"
#include "Config.H"
#include "Debug.H"
#include "IO_Config.H"
#include "Usb.H"
#include "Uart.H"
#include "CH55x_Math.H"
#include "SWO.H"

//#define PIN_DELAY() PIN_DELAY_FAST()

//#define PIN_SWCLK_SET PIN_SWCLK_TCK_SET
//#define PIN_SWCLK_CLR PIN_SWCLK_TCK_CLR



//idata tDAPointer DapData[2] = {0};
//BOOL DapChannel = 0;
volatile UINT8 idata DapState[2] = {0};
volatile UINT8 idata DAP_RequestLength[2] = {0U, 0U};
volatile UINT8 idata DAP_UsbEpoch = 0U;
#if RTX_STACK_DIAG
/* Timer0 updates this single byte atomically; FF means no sample yet.
 * This is free space at RTX checkpoints, not whole-program peak stack.
 */
volatile UINT8 data RTX_StackMinFree = 0xFFU;
#endif

// Clock Macros
#define MAX_SWJ_CLOCK(delay_cycles) \
    ((CPU_CLOCK / 2U) / (IO_PORT_WRITE_CYCLES + delay_cycles))
#define CLOCK_DELAY(swj_clock) \
    (((CPU_CLOCK / 2U) / swj_clock) - IO_PORT_WRITE_CYCLES)
#if DAP_SWJ_CALIBRATED && DELAY_FAST_CYCLES != 0
#error Calibrated profile requires the zero-delay fast backend
#endif
#define SWJ_CLOCK_LIMIT MAX_SWJ_CLOCK(DELAY_FAST_CYCLES)

#if CPU_CLOCK != 24000000UL
#error Compact clock arithmetic requires 24 MHz CPU_CLOCK
#endif

// CMSIS-DAP command/response byte counts (excluding variable payload data).
#define DAP_COMMAND_ID_SIZE                 1U
#define DAP_INFO_REQUEST_SIZE               1U
#define DAP_INFO_RESPONSE_HEADER_SIZE       1U
#define DAP_RESPONSE_STATUS_SIZE            1U
#define DAP_RESPONSE_RESET_TARGET_SIZE      2U
#define DAP_RESPONSE_TRANSFER_SIZE          2U
#define DAP_RESPONSE_TRANSFER_BLOCK_SIZE    3U
#define DAP_RESPONSE_JTAG_IDCODE_SIZE       5U
#define DAP_INFO_OVERFLOW 0xFFU

// Protocol fields stay byte-oriented in persistent state and are assembled only when used.
#define DAP_UINT16_FROM_BYTES(low_byte, high_byte) \
    ((UINT16I)(low_byte) | ((UINT16I)(high_byte) << 8))
#define DAP_TRANSFER_RETRY_COUNT() \
    DAP_UINT16_FROM_BYTES(DAP_Data.transfer.retry_count_l, DAP_Data.transfer.retry_count_h)
#define DAP_TRANSFER_MATCH_RETRY() \
    DAP_UINT16_FROM_BYTES(DAP_Data.transfer.match_retry_l, DAP_Data.transfer.match_retry_h)

xdata DAP_Data_t DAP_Data; // DAP Data

// 命令处理优先体积；位时序模块 SW_DP.C 仍保持工程的 SPEED 配置。
#if DAP_JTAG_COMPACT_DISPATCH
#pragma OPTIMIZE(9, SIZE)
#else
#pragma OPTIMIZE(8, SIZE)
#endif
/* BEGIN DAP_Packet */
// 单包容量由末地址限制，溢出立即停止；不再维护 Reserve/余量/故障标志。
// 只有通过容量比较后才推进指针，因此以下差值始终处于 0..64。
// 状态由 DAP 任务独占，USB ISR 不调用命令执行函数。
static UINT8 idata DAP_RequestEndLow;
static UINT8 idata DAP_ResponseEndLow;
static UINT8 idata DAP_Consumed;
// 每包最多 64 字节，末位置差可用模 256 减法，避免 16 位指针差运算。
#define DAP_POINTER_LOW(ptr) ((UINT8)(UINT16)(ptr))
#define DAP_RX_LEFT(ptr) ((UINT8)(DAP_RequestEndLow - DAP_POINTER_LOW(ptr)))
#define DAP_TX_LEFT(ptr) ((UINT8)(DAP_ResponseEndLow - DAP_POINTER_LOW(ptr)))
#define DAP_RX_AVAILABLE(ptr, size) (DAP_RX_LEFT(ptr) >= (size))
#define DAP_TX_AVAILABLE(ptr, size) (DAP_TX_LEFT(ptr) >= (size))

// 高半字节为固定请求载荷，低半字节为固定响应载荷；不包含命令 ID。
// 变长数据在对应处理循环中检查，不维护第二套传输解析器。
static const UINT8 code DAP_CommandHeaderSize[] = {
    0x11U, 0x21U, 0x11U, 0x01U, // Info、HostStatus、Connect、Disconnect
    0x51U, 0x22U, 0x43U, 0x00U, // TransferConfigure、Transfer、Block、Abort
    0x51U, 0x21U, 0x02U, 0x00U, // WriteABORT、Delay、ResetTarget、未实现
    0x00U, 0x00U, 0x00U, 0x00U,
    0x61U, 0x41U, 0x11U, 0x11U, // SWJ Pins、Clock、Sequence、SWD Configure
    0x11U, 0x11U, 0x15U         // JTAG Sequence、Configure、IDCODE
#if SWO_UART
    ,0x11U, 0x11U, 0x44U, 0x11U, 0x05U, 0x23U, 0x00U, 0x10U
#endif
};
/* END DAP_Packet */

// 请求和响应均位于 EP1 XRAM；使用专用指针避免通用指针分派。
static const UINT8 xdata * request;
static UINT8 xdata * response;

// 全局时钟按 8 位字节保存；仅在配置边界临时组装/拆分 32 位数值。
static void DAP_StoreClock(UINT32 clock)
{
    DAP_Data.nominal_clock[0] = (UINT8)(clock >> 0);
    DAP_Data.nominal_clock[1] = (UINT8)(clock >> 8);
    DAP_Data.nominal_clock[2] = (UINT8)(clock >> 16);
    DAP_Data.nominal_clock[3] = (UINT8)(clock >> 24);
}

// Get DAP Information
//   id:      info identifier
//   info:    pointer to info datas
//   return:  number of bytes in info datas
static UINT8I DAP_Info(UINT8 id, UINT8 xdata *info)
{
    UINT8I length = 0U;

    switch (id) {
    case DAP_ID_VENDOR:
        length = 0;
        break;
    case DAP_ID_PRODUCT:
        length = 0;
        break;
    case DAP_ID_SER_NUM:
        length = 0;
        break;
    case DAP_ID_FW_VER: 
        length = (UINT8)sizeof(DAP_FW_VER);
        if (!DAP_TX_AVAILABLE(info, length)) { return DAP_INFO_OVERFLOW; }
        memcpy(info, DAP_FW_VER, length);
        break;
    case DAP_ID_PRODUCT_FW_VER:
        length = (UINT8)sizeof(TARGET_DEVICE_FW_VER);
        if (!DAP_TX_AVAILABLE(info, length)) { return DAP_INFO_OVERFLOW; }
        memcpy(info, TARGET_DEVICE_FW_VER, length);
        break;
    case DAP_ID_DEVICE_VENDOR:
#if TARGET_DEVICE_FIXED
        length = (UINT8)sizeof(TargetDeviceVendor);
        if (!DAP_TX_AVAILABLE(info, length)) { return DAP_INFO_OVERFLOW; }
        memcpy(info, TargetDeviceVendor, length);
#endif
        break;
    case DAP_ID_DEVICE_NAME:
#if TARGET_DEVICE_FIXED
        length = (UINT8)sizeof(TargetDeviceName);
        if (!DAP_TX_AVAILABLE(info, length)) { return DAP_INFO_OVERFLOW; }
        memcpy(info, TargetDeviceName, length);
#endif
        break;
    case DAP_ID_CAPABILITIES:
        if (!DAP_TX_AVAILABLE(info, 1U)) { return DAP_INFO_OVERFLOW; }
        info[0] = ((DAP_SWD != 0) ? (1U << 0) : 0U)
            | ((DAP_JTAG != 0) ? (1U << 1) : 0U)
            | ((SWO_UART != 0) ? (1U << 2) : 0U)
            | ((SWO_MANCHESTER != 0) ? (1U << 3) : 0U)
            |
            /* Atomic Commands  */ ((DAP_ATOMIC_COMMANDS != 0) ? (1U << 4) : 0U)
            | ((TIMESTAMP_CLOCK != 0U) ? (1U << 5) : 0U)
            | ((SWO_STREAM != 0U) ? (1U << 6) : 0U);
        length = 1U;
        break;
    case DAP_ID_TIMESTAMP_CLOCK:
#if (TIMESTAMP_CLOCK != 0U)
        if (!DAP_TX_AVAILABLE(info, 4U)) { return DAP_INFO_OVERFLOW; }
        info[0] = (UINT8)(TIMESTAMP_CLOCK >>  0);
        info[1] = (UINT8)(TIMESTAMP_CLOCK >>  8);
        info[2] = (UINT8)(TIMESTAMP_CLOCK >> 16);
        info[3] = (UINT8)(TIMESTAMP_CLOCK >> 24);
        length = 4U;
#endif
      break;
    case DAP_ID_SWO_BUFFER_SIZE:
#if ((SWO_UART != 0) || (SWO_MANCHESTER != 0))
        if (!DAP_TX_AVAILABLE(info, 4U)) { return DAP_INFO_OVERFLOW; }
        info[0] = (UINT8)(SWO_BUFFER_SIZE >>  0);
        info[1] = (UINT8)(SWO_BUFFER_SIZE >>  8);
        info[2] = (UINT8)(SWO_BUFFER_SIZE >> 16);
        info[3] = (UINT8)(SWO_BUFFER_SIZE >> 24);
        length = 4U;
#endif
        break;
    case DAP_ID_PACKET_SIZE:
        if (!DAP_TX_AVAILABLE(info, 2U)) { return DAP_INFO_OVERFLOW; }
        info[0] = (UINT8)(DAP_PACKET_SIZE >> 0);
        info[1] = (UINT8)(DAP_PACKET_SIZE >> 8);
        length = 2U;
        break;
    case DAP_ID_PACKET_COUNT:
        if (!DAP_TX_AVAILABLE(info, 1U)) { return DAP_INFO_OVERFLOW; }
        info[0] = DAP_PACKET_COUNT;
        length = 1U;
        break;
    default:
        break;
    }

    return (length);
}


// Process Delay command and prepare response
//   request:  pointer to request data
//   response: pointer to response data
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_Delay(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    UINT16I delay;

    delay = (UINT16)(*(req + 0)) | ((UINT16)(*(req + 1)) << 8);
    // 协议单位为微秒，不能使用 RTX tick；零延时必须立即返回。
    if (delay != 0U) {
        mDelayuS(delay);
    }

    *request_count = 2U;
    *resp = DAP_OK;
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process Host Status command and prepare response
//   request:  pointer to request data
//   response: pointer to response data
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_HostStatus(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    *request_count = 2U;
    // Handle different host status requests
    switch (*req) {
    case DAP_DEBUGGER_CONNECTED:  // Debugger connected / disconnected
        // Use LSB of second byte to control "connected" LED (if implemented)
        LED_CONNECTED_OUT((*(req + 1) & 1U));
        break;
    case DAP_TARGET_RUNNING:      // Target running / halted
        // Use LSB of second byte to control "running" LED (if implemented)
        LED_RUNNING_OUT((*(req + 1) & 1U));
        break;
    default:                      // Unsupported host status type
        *resp = DAP_ERROR;
        return DAP_RESPONSE_STATUS_SIZE;
    }

    *resp = DAP_OK;
    return DAP_RESPONSE_STATUS_SIZE;
}
// Process Connect command and prepare response
//   request:  pointer to request data
//   response: pointer to response data
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_Connect(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    UINT8I port;

    *request_count = 1U;
    if (*req == DAP_PORT_AUTODETECT) {
        // 主机请求自动探测时使用固件默认调试端口。
        port = DAP_DEFAULT_PORT;
    } else {
        // 主机显式指定端口时按请求选择 SWD/JTAG。
        port = *req;
    }

    switch (port) {
#if (DAP_SWD != 0)
    case DAP_PORT_SWD:
#if DAP_JTAG
        JTAG_PINS_OFF(); // Release previous TDI/nTRST before entering SWD.
#endif
        DAP_Data.debug_port = DAP_PORT_SWD;
        PORT_SWD_SETUP();
        RESET_PIN_SETUP();
        break;
#endif
#if (DAP_JTAG != 0)
    case DAP_PORT_JTAG:
#if DAP_GPIO_SWJ_SWO
        SWO_Reset(); // PB3 becomes target TDO, not a SWO trace stream.
#endif
        DAP_Data.debug_port = DAP_PORT_JTAG;
        PORT_JTAG_SETUP();
        RESET_PIN_SETUP();
        break;
#endif
    default:
        DAP_Data.debug_port = DAP_PORT_DISABLED;
        PORT_OFF();
        port = DAP_PORT_DISABLED;
        break;
    }

    *resp = (UINT8)port;
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process Disconnect command and prepare response
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_Disconnect(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    req = req;
    *request_count = 0U;
    DAP_Data.debug_port = DAP_PORT_DISABLED;
    PORT_OFF();
#if SWO_UART
    SWO_Reset();
#endif

    *resp = DAP_OK;
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process Reset Target command and prepare response
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_ResetTarget(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    req = req;
    *request_count = 0U;
    // 本探针未实现目标专属复位序列；主机继续执行 Cortex-M 软件复位。
    // 不把空的 GPIO 宏调用伪装为已经执行的目标复位。
    *(resp + 1) = 0U;
    *(resp + 0) = DAP_OK;
    return DAP_RESPONSE_RESET_TARGET_SIZE;
}

// Process SWJ Pins command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_ReadSWJPins(void)
{
    return (UINT8I)(((UINT8)PIN_SWCLK_TCK_IN() << DAP_SWJ_SWCLK_TCK) |
                    ((UINT8)PIN_SWDIO_TMS_IN() << DAP_SWJ_SWDIO_TMS) |
#if DAP_JTAG
                    ((UINT8)PIN_TDI_IN() << DAP_SWJ_TDI) |
                    ((UINT8)PIN_TDO_IN() << DAP_SWJ_TDO) |
#if DAP_HAS_NTRST
                    ((UINT8)PIN_nTRST_IN() << DAP_SWJ_nTRST) |
#endif
#endif
                    ((UINT8)PIN_nRESET_IN() << DAP_SWJ_nRESET));
}

static UINT8I DAP_SWJ_Pins(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
    UINT8I value;
    UINT8I select;
    UINT8I sample;
    UINT16I slice;
    UINT32 wait;

    *request_count = 6U;
    value = req[0];
    // SWD-only 构建仅驱动真实存在的引脚，避免猜测未接线的复位信号。
    select = req[1] & (DAP_SWJ_SWCLK_TCK_BIT | DAP_SWJ_SWDIO_TMS_BIT);
#if DAP_JTAG
    if (DAP_Data.debug_port == DAP_PORT_JTAG) {
        select |= req[1] & DAP_SWJ_TDI_BIT;
        if (select & DAP_SWJ_TDI_BIT) { PIN_TDI_OUT(value >> DAP_SWJ_TDI); }
#if DAP_HAS_NTRST
        select |= req[1] & DAP_SWJ_nTRST_BIT;
        if (select & DAP_SWJ_nTRST_BIT) { PIN_nTRST_OUT(value >> DAP_SWJ_nTRST); }
#endif
    }
#endif
#if (DAP_HAS_NRESET != 0)
    select |= req[1] & DAP_SWJ_nRESET_BIT;
#endif
    wait = (UINT32)req[2] | ((UINT32)req[3] << 8) |
           ((UINT32)req[4] << 16) | ((UINT32)req[5] << 24);
    if (wait > 3000000UL) {
        wait = 3000000UL;
    }
    if (select & DAP_SWJ_SWCLK_TCK_BIT) {
        if (value & DAP_SWJ_SWCLK_TCK_BIT) {
            PIN_SWCLK_TCK_SET();
        } else {
            PIN_SWCLK_TCK_CLR();
        }
    }
    if (select & DAP_SWJ_SWDIO_TMS_BIT) {
#if DAP_JTAG
        if (DAP_Data.debug_port != DAP_PORT_JTAG)
#endif
        { PIN_SWDIO_OUT_ENABLE(); }
        if (value & DAP_SWJ_SWDIO_TMS_BIT) {
            PIN_SWDIO_TMS_SET();
        } else {
            PIN_SWDIO_TMS_CLR();
        }
    }
#if (DAP_HAS_NRESET != 0)
    if (select & DAP_SWJ_nRESET_BIT) {
        PIN_nRESET_OUT(value >> DAP_SWJ_nRESET);
    }
#endif
    // 回读实际引脚；每次最多等待 1 ms，选中的引脚已匹配时立即返回。
    sample = DAP_ReadSWJPins();
    while (wait && ((sample ^ value) & select)) {
        slice = (wait > 1000UL) ? 1000U : (UINT16I)wait;
        mDelayuS(slice);
        wait -= slice;
        sample = DAP_ReadSWJPins();
    }
    *resp = sample;
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process SWJ Clock command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_SWJ_Clock(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
  UINT32 clock;
  UINT32 delay;
#endif

  *request_count = 4U;

#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
  // C51 的 int 为 16 位：必须先扩宽每个字节，再左移到协议要求的位置。
  clock = ((UINT32)(*(req + 0)) <<  0) |
          ((UINT32)(*(req + 1)) <<  8) |
          ((UINT32)(*(req + 2)) << 16) |
          ((UINT32)(*(req + 3)) << 24);

  if (clock == 0U) {
    *resp = DAP_ERROR;
    return DAP_RESPONSE_STATUS_SIZE;
  }
  // 请求最快配置时延时为零，进入完整的 Fast 协议路径。
  if (clock > SWJ_CLOCK_LIMIT) {
    clock = SWJ_CLOCK_LIMIT;
  }

  DAP_StoreClock(clock);
#if DAP_SWJ_CALIBRATED
  // 保留原 Fast 分界；其他请求保持非零慢档计数。
  // 半周期固定开销保守取 24 clocks，循环迭代实测约 14 clocks。
  delay = (clock > MAX_SWJ_CLOCK(1U)) ? 0UL :
          CH55x_ClockDivide(clock | CH55X_CLOCK_SWJ_CALIBRATED);
#else
  delay = CH55x_ClockDivide(clock | CH55X_CLOCK_SWJ) - IO_PORT_WRITE_CYCLES;
#endif
  // 先使用完整计算结果做饱和，再缩窄到实际延时循环的 16 位参数。
  if (delay > 65535UL) {
    delay = 65535UL;
  }
  DAP_Data.clock_delay = (UINT16)delay;
  DAP_Data.fast_clock = (DAP_Data.clock_delay <= DELAY_FAST_CYCLES) ? 1U : 0U;
  *resp = DAP_OK;
#else
  *resp = DAP_ERROR;
#endif

  return DAP_RESPONSE_STATUS_SIZE;
}

// Process SWJ Sequence command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_SWJ_Sequence(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
  UINT16I count;
  UINT8I data_count;

  count = *req++;
  if (count == 0U) {
    // CMSIS-DAP 规定计数字节为 0 时表示发送 256 bit。
    count = 256U;
  }

  data_count = (UINT8I)((count + 7U) >> 3);
  if (!DAP_RX_AVAILABLE(req, data_count)) { return 0U; }
#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
  SWJ_Sequence(count, req);
  *resp = DAP_OK;
#else
  *resp = DAP_ERROR;
#endif

  *request_count = data_count + 1U;

  return DAP_RESPONSE_STATUS_SIZE;
}



// Process SWD Configure command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_SWD_Configure(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *request_count)
{
#if (DAP_SWD != 0)
  UINT8 value;
#endif

  *request_count = 1U;
#if (DAP_SWD != 0)
  value = *req;
  DAP_Data.mode_cfg.swd.turnaround = (value & 0x03U) + 1U;
  DAP_Data.mode_cfg.swd.data_phase = (value & 0x04U) ? 1U : 0U;

  *resp = DAP_OK;
#else
  *resp = DAP_ERROR;
#endif

  return DAP_RESPONSE_STATUS_SIZE;
}


// Process SWD Sequence command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   consumed_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
// 每个序列先包含一个 info 字节；DOUT 随后携带数据，DIN 则把采样数据追加到响应。
static UINT8I DAP_SWD_Sequence(const UINT8 xdata * req, UINT8 xdata * resp, UINT8I *consumed_count)
{
    UINT8I sequence_info;
    UINT8I sequence_count;
    UINT8I request_count;
    UINT8I response_count;
    UINT8I bit_count;
    UINT8I byte_count;

#if (DAP_SWD != 0)
    *resp++ = DAP_OK;
#else
    *resp++ = DAP_ERROR;
#endif

    request_count = 1U;
    response_count = 1U;

    sequence_count = *req++;
    while (sequence_count--) {
        if (!DAP_RX_AVAILABLE(req, 1U)) { goto sequence_full; }
        sequence_info = *req++;
        bit_count = sequence_info & SWD_SEQUENCE_CLK;
        if (bit_count == 0U) {
            bit_count = 64U;
        }
        byte_count = (bit_count + 7U) / 8U;
        if (sequence_info & SWD_SEQUENCE_DIN) {
            if (!DAP_TX_AVAILABLE(resp, byte_count)) { goto sequence_full; }
        } else if (!DAP_RX_AVAILABLE(req, byte_count)) { goto sequence_full; }

#if (DAP_SWD != 0)
        // 单段序列的方向和位时序由 SW_DP.C 负责。
        SWD_Sequence(sequence_info, req, resp);
#endif

        if ((sequence_info & SWD_SEQUENCE_DIN) != 0U) {
            request_count++;
#if (DAP_SWD != 0)
            resp += byte_count;
            response_count += byte_count;
#endif
        } else {
            req += byte_count;
            request_count += byte_count + 1U;
        }
    }

#if (DAP_SWD != 0)
    // 命令结束后恢复输出高电平，保持 SWD 总线空闲并方便后续传输接管。
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);
#endif

    *consumed_count = request_count;
    return response_count;

sequence_full:
#if (DAP_SWD != 0)
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);
#endif
    return 0U; // 包已满，停止接收/发送剩余序列。
}


// Process Transfer Configure command and prepare response
#if DAP_JTAG
static UINT8I DAP_JTAG_Sequence(const UINT8 xdata *req, UINT8 xdata *resp, UINT8I *consumed)
{
    const UINT8 xdata *head;
    UINT8 xdata *out;
    UINT8I count, info, bytes;
    head = req;
    out = resp;
    *resp++ = DAP_Data.debug_port == DAP_PORT_JTAG ? DAP_OK : DAP_ERROR;
    count = *req++;
    while (count--) {
        if (!DAP_RX_AVAILABLE(req, 1U)) { return 0U; }
        info = *req++;
        bytes = info & JTAG_SEQUENCE_TCK;
        bytes = bytes ? (bytes + 7U) / 8U : 8U;
        if (!DAP_RX_AVAILABLE(req, bytes)) { return 0U; }
        if (info & JTAG_SEQUENCE_TDO) {
            if (!DAP_TX_AVAILABLE(resp, bytes)) { return 0U; }
        }
        if (*out == DAP_OK) {
            JTAG_Sequence(info, req, resp);
#if DAP_SPI_JTAG
            if (DAP_SPI_Failed) { *out = DAP_ERROR; }
#endif
        }
        req += bytes;
        if (*out == DAP_OK && (info & JTAG_SEQUENCE_TDO)) { resp += bytes; }
    }
    *consumed = (UINT8I)(req - head);
    return (UINT8I)(resp - out);
}

static UINT8I DAP_JTAG_Configure(const UINT8 xdata *req, UINT8 xdata *resp, UINT8I *consumed)
{
    UINT8I count, i;
    count = *req++;
    if (!DAP_RX_AVAILABLE(req, count)) { return 0U; }
    *consumed = count + 1U;
    *resp = DAP_ERROR;
    if (count > DAP_JTAG_DEV_CNT) { return DAP_RESPONSE_STATUS_SIZE; }
    for (i = 0U; i < count; i++) {
        if (!req[i] || req[i] > 32U) { return DAP_RESPONSE_STATUS_SIZE; }
    }
    for (i = 0U; i < count; i++) { DAP_Data.mode_cfg.jtag.ir_length[i] = req[i]; }
    DAP_Data.mode_cfg.jtag.count = count;
    *resp = DAP_OK;
    return DAP_RESPONSE_STATUS_SIZE;
}

static UINT8I DAP_JTAG_Select(UINT8I index)
{
    if (index >= DAP_Data.mode_cfg.jtag.count) { return 0U; }
    if (DAP_Data.mode_cfg.jtag.ir_length[index] < 4U) { return 0U; } // ARM instructions need four bits.
    DAP_Data.mode_cfg.jtag.index = index;
    return 1U;
}

static UINT8I DAP_JTAG_IDCode(const UINT8 xdata *req, UINT8 xdata *resp, UINT8I *consumed)
{
    UINT8 idata word[4];
    *consumed = 1U;
    memset(word, 0, sizeof(word));
    *resp = DAP_ERROR;
    if (DAP_Data.debug_port == DAP_PORT_JTAG && DAP_JTAG_Select(*req)) {
        JTAG_ReadIDCode(word);
        *resp = DAP_OK;
#if DAP_SPI_JTAG
        if (DAP_SPI_Failed) { *resp = DAP_ERROR; }
#endif
    }
    resp[1] = word[0]; resp[2] = word[1]; resp[3] = word[2]; resp[4] = word[3];
    return DAP_RESPONSE_JTAG_IDCODE_SIZE;
}
#endif

// Process Transfer Configure command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_TransferConfigure(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *request_count)
{
    DAP_Data.transfer.idle_cycles = *(req + 0);
    // retry_count 和 match_retry 均为 CMSIS-DAP 小端 16 位字段。
    DAP_Data.transfer.retry_count_l = *(req + 1);
    DAP_Data.transfer.retry_count_h = *(req + 2);
    DAP_Data.transfer.match_retry_l = *(req + 3);
    DAP_Data.transfer.match_retry_h = *(req + 4);

    *request_count = 5U;
    *res = DAP_OK;
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process SWD Transfer command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   consumed_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID

// USB ISR 只设置这个 8 位标志，传输循环通过 volatile 访问及时观察中止。
volatile UINT8 idata DAP_TransferAbort_Flag = 0U;
// 四字节传输保持小端字节顺序；明确区分协议 XRAM 与临时 IDATA。
// 先赋值再推进指针，便于 C51 保持 DPTR/R0；合并后递增表达式反而更大。
// 普通 MOVX，不切换 DPS/自动递增模式，不新增关中断区间。
static void DAP_LoadWord(const UINT8 xdata *source, UINT8I *destination)
{
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
}

static void DAP_StoreWord(UINT8 xdata *destination, const UINT8I *source)
{
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
    destination++;
    source++;
    *destination = *source;
}

#if (ENABLE_DAP_MATCH != 0)
// 匹配循环和最终状态共用同一掩码判定，仍逐字节处理 32 位数据。
static UINT8I DAP_ValueMismatch(const UINT8I *datas, const UINT8I *match_value)
{
    UINT8I i;
    for (i = 0U; i < 4U; i++) {
        if ((datas[i] & DAP_Data.transfer.match_mask[i]) != match_value[i]) {
            return 1U;
        }
    }
    return 0U;
}
#endif

// 共用 WAIT 重试流程；每条总线访问独立获得配置的重试预算。
// 保持先执行一次、最多追加 retry_count 次以及中止观察顺序不变。
static UINT8I DAP_SWD_TransferRetry(UINT8I request_value, UINT8I *datas)
{
    UINT16I retry;
    UINT8I response_value;
    retry = DAP_TRANSFER_RETRY_COUNT();
    do {
#if DAP_JTAG
        if (DAP_Data.debug_port == DAP_PORT_JTAG) { response_value = JTAG_Transfer(request_value, datas); }
        else
#endif
        { response_value = SWD_Transfer(request_value, datas); }
    } while ((response_value == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort_Flag);
    return response_value;
}

static UINT8I DAP_SWD_Transfer(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *consumed_count)
{
#if (DAP_SWD != 0)
    const UINT8 xdata *request_head;
    const UINT8 xdata *transfer_data;
    UINT8 xdata *response_head;
    UINT8 idata datas[4];
    UINT8 idata match_value[4]; // 数组显式限定 IDATA，供公共字节函数访问。
    UINT8I post_read;
    UINT8I check_write;
    UINT8I request_count;
    UINT8I request_value;
    UINT8I response_count;
    UINT8I response_value;
#if (ENABLE_DAP_MATCH != 0)
    UINT16I match_retry;
#endif

    request_head = req;
    response_head = res;
    res += 2;
    response_count = 0U;
    response_value = 0U;
    post_read = 0U;
    check_write = 0U;

    req++; // Ignore DAP index

    request_count = *req++;
    while (request_count != 0U)
    {
        // 先消费本条请求及其载荷；提前失败后只跳过尚未开始的请求。
        request_count--;
        if (!DAP_RX_AVAILABLE(req, 1U)) { return 0U; }
        request_value = *req++;
        transfer_data = req;
        if (((request_value & DAP_TRANSFER_RnW) == 0U) ||
            ((request_value & DAP_TRANSFER_MATCH_VALUE) != 0U)) {
            if (!DAP_RX_AVAILABLE(req, 4U)) { return 0U; }
            req += 4;
        }
        // AP 前一结果尚未写回时占四字节；本次普通读也需要四字节。
        // 这里只比较容量，不扣减预留；提前总线失败后无需归还容量。
        if (!DAP_TX_AVAILABLE(res, (post_read ? 4U : 0U) +
            (((request_value & DAP_TRANSFER_RnW) && !(request_value & DAP_TRANSFER_MATCH_VALUE)) ? 4U : 0U))) {
            return 0U;
        }
#if (TIMESTAMP_CLOCK == 0U)
        // 不支持时间戳时明确拒绝请求，不能回传缺少时间戳的成功响应。
        if (request_value & DAP_TRANSFER_TIMESTAMP) {
            response_value = DAP_TRANSFER_ERROR;
            break;
        }
#endif
        if ((request_value & DAP_TRANSFER_RnW) != 0U)
        {
            // Read registers
            if (post_read)
            {
                // Read was posted before
                if ((request_value & (DAP_TRANSFER_APnDP | DAP_TRANSFER_MATCH_VALUE)) == DAP_TRANSFER_APnDP)
                {
                    // AP 读为 posted-read：取回上一次结果，同时发起下一次 AP 读。
                    response_value = DAP_SWD_TransferRetry(request_value, datas);
                }
                else
                {
                    // 请求类型改变时，通过 DP RDBUFF 取回挂起的 AP 读结果。
                    response_value = DAP_SWD_TransferRetry(DP_RDBUFF | DAP_TRANSFER_RnW, datas);
                    post_read = 0U;
                }
                if (response_value != DAP_TRANSFER_OK)
                {
                    break;
                }
                // Store previous AP data
                DAP_StoreWord(res, datas);
                res += 4U;
            }

            if ((request_value & DAP_TRANSFER_MATCH_VALUE) != 0U)
            {
                // Read with value match
                DAP_LoadWord(transfer_data, match_value);


#if (ENABLE_DAP_MATCH != 0)
                match_retry = DAP_TRANSFER_MATCH_RETRY();
                if ((request_value & DAP_TRANSFER_APnDP) != 0U)
                {
                    // Post AP read
                    response_value = DAP_SWD_TransferRetry(request_value, NULL);
                    if (response_value != DAP_TRANSFER_OK)
                    {
                        break;
                    }
                }

                do
                {
                    // Read until value match or retries exhausted
                    response_value = DAP_SWD_TransferRetry(request_value, datas);
                    if (response_value != DAP_TRANSFER_OK)
                    {
                        break;
                    }
                }
                while (DAP_ValueMismatch(datas, match_value) &&
                        match_retry-- && !DAP_TransferAbort_Flag);

                if ((response_value == DAP_TRANSFER_OK) &&
                    DAP_ValueMismatch(datas, match_value))
                {
                    response_value |= DAP_TRANSFER_MISMATCH;
                }
#else
                response_value = DAP_TRANSFER_ERROR;
#endif
                if (response_value != DAP_TRANSFER_OK)
                {
                    break;
                }
            }
            else
            {
                // Normal read
                if ((request_value & DAP_TRANSFER_APnDP) != 0U)
                {
                    // Read AP register (posted read sequence)
                    if (post_read == 0U)
                    {
                        response_value = DAP_SWD_TransferRetry(request_value, NULL);
                        if (response_value != DAP_TRANSFER_OK)
                        {
                            break;
                        }
                        post_read = 1U;
                    }
                }
                else
                {
                    // Read DP register
                    response_value = DAP_SWD_TransferRetry(request_value, datas);
                    if (response_value != DAP_TRANSFER_OK)
                    {
                        break;
                    }

                    DAP_StoreWord(res, datas);
                    res += 4U;
                }
            }
            check_write = 0U;
        }
        else
        {
            // Write register
            if (post_read)
            {
                // Drain previously posted AP read
                response_value = DAP_SWD_TransferRetry(DP_RDBUFF | DAP_TRANSFER_RnW, datas);
                if (response_value != DAP_TRANSFER_OK)
                {
                    break;
                }
                DAP_StoreWord(res, datas);
                res += 4U;
                post_read = 0U;
            }

            // Load write data
            DAP_LoadWord(transfer_data, datas);
            if ((request_value & DAP_TRANSFER_MATCH_MASK) != 0U)
            {
#if (ENABLE_DAP_MATCH != 0)
                // Write match mask
                DAP_Data.transfer.match_mask[0] = datas[0];
                DAP_Data.transfer.match_mask[1] = datas[1];
                DAP_Data.transfer.match_mask[2] = datas[2];
                DAP_Data.transfer.match_mask[3] = datas[3];
                response_value = DAP_TRANSFER_OK;
#else
                response_value = DAP_TRANSFER_ERROR;
#endif
            }
            else
            {
                // Write DP/AP register
                response_value = DAP_SWD_TransferRetry(request_value, datas);
                if (response_value != DAP_TRANSFER_OK)
                {
                    break;
                }
                check_write = 1U;
            }
        }

        response_count++;
        if (DAP_TransferAbort_Flag)
        {
            break;
        }
    }

    // Skip remaining requests if transfer ended early
    for (; request_count != 0U; request_count--)
    {
        if (!DAP_RX_AVAILABLE(req, 1U)) { return 0U; }
        request_value = *req++;
        if ((request_value & DAP_TRANSFER_RnW) != 0U)
        {
            if ((request_value & DAP_TRANSFER_MATCH_VALUE) != 0U)
            {
                if (!DAP_RX_AVAILABLE(req, 4U)) { return 0U; }
                req += 4;
            }
        }
        else
        {
            if (!DAP_RX_AVAILABLE(req, 4U)) { return 0U; }
            req += 4;
        }
    }

    if (response_value == DAP_TRANSFER_OK)
    {
        if (post_read)
        {
            // Complete last posted AP read
            response_value = DAP_SWD_TransferRetry(DP_RDBUFF | DAP_TRANSFER_RnW, datas);
            if (response_value == DAP_TRANSFER_OK)
            {
                DAP_StoreWord(res, datas);
                res += 4U;
            }
        }
        else if (check_write)
        {
            // Check last write
            response_value = DAP_SWD_TransferRetry(DP_RDBUFF | DAP_TRANSFER_RnW, NULL);
        }
    }

    *(response_head + 0) = (UINT8)response_count;
    *(response_head + 1) = (UINT8)response_value;

    *consumed_count = (UINT8I)(req - request_head);
    return (UINT8I)(res - response_head);
#else
    req = req;
    *consumed_count = 2U;
    *(res + 0) = 0U;
    *(res + 1) = DAP_ERROR;
    return DAP_RESPONSE_TRANSFER_SIZE;
#endif
}

// Process Dummy Transfer command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   consumed_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_Dummy_Transfer(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *consumed_count)
{
    const UINT8 xdata *request_head;
    UINT8 request_count;
    UINT8 request_value;

    request_head = req;
    req++; // Ignore DAP index
    request_count = *req++;

    for (; request_count != 0U; request_count--)
    {
        if (!DAP_RX_AVAILABLE(req, 1U)) { return 0U; }
        request_value = *req++;
        if ((request_value & DAP_TRANSFER_RnW) != 0U)
        {
            if ((request_value & DAP_TRANSFER_MATCH_VALUE) != 0U)
            {
                if (!DAP_RX_AVAILABLE(req, 4U)) { return 0U; }
                req += 4;
            }
        }
        else
        {
            if (!DAP_RX_AVAILABLE(req, 4U)) { return 0U; }
            req += 4;
        }
    }

    *(res + 0) = 0U; // response count
    *(res + 1) = 0U; // response value

    *consumed_count = (UINT8I)(req - request_head);
    return DAP_RESPONSE_TRANSFER_SIZE;
}

// Process Transfer command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_Transfer(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *request_count)
{
    DAP_TransferAbort_Flag = 0U;

    switch (DAP_Data.debug_port)
    {
#if (DAP_SWD != 0)
    case DAP_PORT_SWD:
        return DAP_SWD_Transfer(req, res, request_count);
#endif
#if DAP_JTAG
    case DAP_PORT_JTAG:
        if (DAP_JTAG_Select(req[0])) { return DAP_SWD_Transfer(req, res, request_count); }
        // Invalid index still consumes all framed payload through the dummy path.
#endif
    default:
        return DAP_Dummy_Transfer(req, res, request_count);
    }
}

// Process SWD Transfer Block command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   consumed_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_SWD_TransferBlock(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *consumed_count)
{
#if (DAP_SWD != 0)
    const UINT8 xdata *request_head;
    UINT8 xdata *response_head;
    UINT8 idata datas[4];
    UINT8I request_count;
    UINT8I request_value;
    UINT8I response_count;
    UINT8I response_value;
    UINT8I processed;
    UINT8I transfer_request;
#if DAP_SWD_BLOCK_DIRECT
    UINT16I retry;
#endif

    request_head = req;
    response_head = res;
    res += 3;
    response_count = 0U;
    response_value = 0U;

    req++; // Ignore DAP index
    // DAP_TransferBlock 护栏已确认高字节为零且单包最多容纳 15 个读结果。
    request_count = *req;
    req += 2;
    // 即使次数为零，TransferBlock 的固定请求头仍包含 request 字节。
    request_value = *req++;
    if (request_count == 0U)
    {
        goto done;
    }

    if ((request_value & DAP_TRANSFER_RnW) != 0U)
    {
        // Read register block
        if ((request_value & DAP_TRANSFER_APnDP) != 0U)
        {
            // Post AP read
            response_value = DAP_SWD_TransferRetry(request_value, NULL);
            if (response_value != DAP_TRANSFER_OK)
            {
                goto done;
            }
        }

        for (processed = 0U; processed < request_count; processed++)
        {
            transfer_request = request_value;
            if ((processed == (request_count - 1U)) && ((request_value & DAP_TRANSFER_APnDP) != 0U))
            {
                // Last AP read must use RDBUFF
                transfer_request = DP_RDBUFF | DAP_TRANSFER_RnW;
            }

            response_value = DAP_SWD_TransferRetry(transfer_request, datas);
            if (response_value != DAP_TRANSFER_OK)
            {
                goto done;
            }

            DAP_StoreWord(res, datas);
            res += 4U;
            response_count++;
            if (DAP_TransferAbort_Flag)
            {
                break;
            }
        }
    }
    else
    {
        // Write register block
        processed = 0U;
        while (processed < request_count)
        {
            DAP_LoadWord(req, datas);
            req += 4;
            processed++;

#if DAP_SWD_BLOCK_DIRECT
            // 成功路径直接传输，WAIT 时才准备重试预算。
            response_value = SWD_Transfer(request_value, datas);
            if (response_value == DAP_TRANSFER_WAIT) {
                // 每字仍最多追加配置次数；Abort 的观察顺序保持不变。
                retry = DAP_TRANSFER_RETRY_COUNT();
                while ((response_value == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort_Flag) {
                    response_value = SWD_Transfer(request_value, datas);
                }
            }
#else
            response_value = DAP_SWD_TransferRetry(request_value, datas);
#endif
            if (response_value != DAP_TRANSFER_OK)
            {
                break;
            }

            response_count++;
            if (DAP_TransferAbort_Flag)
            {
                break;
            }
        }

        // Skip remaining write payload bytes if transfer ended early
        if (processed < request_count)
        {
            req += (UINT8I)(request_count - processed) * 4U;
        }

        // Check last write
        if ((response_value == DAP_TRANSFER_OK) && !DAP_TransferAbort_Flag)
        {
            response_value = DAP_SWD_TransferRetry(DP_RDBUFF | DAP_TRANSFER_RnW, NULL);
        }
    }

done:
    *(response_head + 0) = (UINT8)(response_count >> 0);
    *(response_head + 1) = 0U; // 协议保留计数高字节，单包计数不会超过 255。
    *(response_head + 2) = (UINT8)response_value;

    *consumed_count = (UINT8I)(req - request_head);
    return (UINT8I)(res - response_head);
#else
    req = req;
    *consumed_count = 4U;
    *(res + 0) = 0U;
    *(res + 1) = 0U;
    *(res + 2) = DAP_ERROR;
    return DAP_RESPONSE_TRANSFER_BLOCK_SIZE;
#endif
}

// Process Transfer Block command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_TransferBlock(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *request_count)
{
    // 固定头已检查，高计数字节或块载荷/结果超过单包容量时停止。
    if (req[2] != 0U) { return 0U; }
    if (req[3] & DAP_TRANSFER_RnW) {
        if (req[1] > DAP_TX_LEFT(res + 3U) / 4U) { return 0U; }
    } else {
        if (req[1] > DAP_RX_LEFT(req + 4U) / 4U) { return 0U; }
    }
    DAP_TransferAbort_Flag = 0U;

    switch (DAP_Data.debug_port)
    {
#if (DAP_SWD != 0)
    case DAP_PORT_SWD:
        return DAP_SWD_TransferBlock(req, res, request_count);
#endif
#if DAP_JTAG
    case DAP_PORT_JTAG:
        if (DAP_JTAG_Select(req[0])) { return DAP_SWD_TransferBlock(req, res, request_count); }
#endif
    default:
    {
        const UINT8 xdata *request_head;
        UINT8I transfer_count;
        UINT8I request_value;

        request_head = req;
        req++; // Ignore DAP index
        transfer_count = *req; // 入口护栏已验证高字节为零。
        req += 2;
        request_value = *req++;
        if ((request_value & DAP_TRANSFER_RnW) == 0U)
        {
            req += transfer_count * 4U;
        }

        *(res + 0) = 0U; // response count [7:0]
        *(res + 1) = 0U; // response count [15:8]
        *(res + 2) = 0U; // response value

        *request_count = (UINT8I)(req - request_head);
        return DAP_RESPONSE_TRANSFER_BLOCK_SIZE;
    }
    }
}

// Process SWD Write ABORT command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_SWD_WriteAbort(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *request_count)
{
#if (DAP_SWD != 0)
    UINT8 idata datas[4];
    UINT8I ack;
#endif

    *request_count = 5U;
#if (DAP_SWD != 0)
    DAP_LoadWord(req + 1U, datas);

    // WriteABORT 只在目标 ACK=OK 时报告成功，WAIT/FAULT/协议错误均返回错误。
    ack = SWD_Transfer(DP_ABORT, datas);
    *res = (ack == DAP_TRANSFER_OK) ? DAP_OK : DAP_ERROR;
#else
    req = req;
    *res = DAP_ERROR;
#endif
    return DAP_RESPONSE_STATUS_SIZE;
}

// Process Write ABORT command and prepare response
//   request:  pointer to request datas
//   response: pointer to response datas
//   request_count: 消耗的请求载荷字节数，不含命令 ID
//   return: 响应载荷字节数，不含命令 ID
static UINT8I DAP_WriteAbort(const UINT8 xdata *req, UINT8 xdata *res, UINT8I *request_count)
{
    switch (DAP_Data.debug_port) {
#if (DAP_SWD != 0)
    case DAP_PORT_SWD:
        return DAP_SWD_WriteAbort(req, res, request_count);
#endif
#if DAP_JTAG
    case DAP_PORT_JTAG:
    {
        UINT8 idata word[4];
        *request_count = 5U;
        *res = DAP_ERROR;
        if (DAP_JTAG_Select(req[0])) {
            DAP_LoadWord(req + 1U, word);
            JTAG_WriteAbort(word);
            *res = DAP_OK;
#if DAP_SPI_JTAG
            if (DAP_SPI_Failed) { *res = DAP_ERROR; }
#endif
        }
        return DAP_RESPONSE_STATUS_SIZE;
    }
#endif
    default:
        *res = DAP_ERROR;
        *request_count = 5U;
        return DAP_RESPONSE_STATUS_SIZE;
    }
}


// Process DAP Vendor command request and prepare response
// Default function (can be overridden)
//   request:  pointer to request data
//   response: pointer to response data
//   return:   8 位响应字节数（含命令 ID），零表示容量不足。
//   request:  消耗量由 DAP_Consumed 保存，由组合命令单独推进。
UINT8I DAP_ProcessVendorCommand()
{
#if CDC_DIAGNOSTICS
#if CDC_RX_FIFO
    bit saved_ea;
    UINT8I diag_index;
    const volatile UINT8 idata *diag_cursor;
#if CDC_RX_SNAPSHOT_COMPACT
    UINT8I diag_limit;
#endif
#else
    UINT8I saved_ea;
#endif
#if (CDC_IN_SNAPSHOT_DIAG || CDC_RX_SNAPSHOT_DIAG) && !CDC_RX_SNAPSHOT_COMPACT
    UINT8I snapshot_index;
#endif
    if (*request == CDC_DIAG_COMMAND) {
        if (!DAP_TX_AVAILABLE(response, CDC_DIAG_RESPONSE_SIZE)) { return 0U; }
        // 只读诊断快照，不清计数，不动 CDC 通道；主机在回环前后查询。
        saved_ea = EA;
        EA = 0;
        response[0] = CDC_DIAG_COMMAND;
        response[1] = CDC_DIAG_VERSION; // 诊断格式版本。
        response[2] = CDC.flags;
#if CDC_RX_FIFO
        // Existing byte counters are contiguous; retain the v2 wire layout.
        diag_cursor = &CDC.rx_length;
#if CDC_RX_SNAPSHOT_COMPACT
        diag_limit = (CDC.flags & (1U << CDC_RX_OVERFLOW_BIT)) ? CDC_DIAG_RESPONSE_SIZE : 14U;
        for (diag_index = 3U; diag_index < diag_limit; diag_index++) {
            response[diag_index] = *diag_cursor++;
        }
        for (; diag_index < CDC_DIAG_RESPONSE_SIZE; diag_index++) {
            response[diag_index] = 0U;
        }
#else
        for (diag_index = 3U; diag_index < 14U; diag_index++) {
            response[diag_index] = *diag_cursor++;
        }
#endif
#else
        response[3] = CDC.rx_length;
        response[4] = CDC.rx_dropped[0];
        response[5] = CDC.rx_dropped[1];
        response[6] = CDC.usb_out_bytes[0];
        response[7] = CDC.usb_out_bytes[1];
        response[8] = CDC.uart_tx_bytes[0];
        response[9] = CDC.uart_tx_bytes[1];
        response[10] = CDC.uart_rx_bytes[0];
        response[11] = CDC.uart_rx_bytes[1];
        response[12] = CDC.usb_in_bytes[0];
        response[13] = CDC.usb_in_bytes[1];
#endif
#if (CDC_IN_SNAPSHOT_DIAG || CDC_RX_SNAPSHOT_DIAG) && !CDC_RX_SNAPSHOT_COMPACT
        for (snapshot_index = 0U; snapshot_index < (CDC_DIAG_RESPONSE_SIZE - 14U); snapshot_index++) {
            // 无有效标志时返回零，不泄露 USB reset 前的旧快照。
            response[14U + snapshot_index] =
#if CDC_RX_SNAPSHOT_DIAG
                (CDC.flags & (1U << CDC_RX_OVERFLOW_BIT)) ? CDC.rx_snapshot[snapshot_index] : 0U;
#else
                (CDC.flags & (1U << CDC_RX_CHANNEL_MISMATCH_BIT)) ? CDC.in_snapshot[snapshot_index] : 0U;
#endif
        }
#endif
        EA = saved_ea;
        return CDC_DIAG_RESPONSE_SIZE;
    }
#endif
//    (void)request;
// GPIO 快照只读，不更改输出、电平模式或外设复用；各字段不是同步采样。
#if RTX_STACK_DIAG
    if (*request == RTX_STACK_DIAG_COMMAND) {
        if (!DAP_TX_AVAILABLE(response, RTX_STACK_DIAG_RESPONSE_SIZE)) { return 0U; }
        response[0] = RTX_STACK_DIAG_COMMAND;
        response[1] = 1U;
        response[2] = RTX_StackMinFree;
        return RTX_STACK_DIAG_RESPONSE_SIZE;
    }
#endif
#if DAP_GPIO_DIAGNOSTICS
    if (*request == DAP_GPIO_DIAG_COMMAND) {
        if (!DAP_TX_AVAILABLE(response, DAP_GPIO_DIAG_RESPONSE_SIZE)) { return 0U; }
        response[0] = DAP_GPIO_DIAG_COMMAND;
        response[1] = 1U;
        response[2] = P1;
        response[3] = P3;
        response[4] = P1_MOD_OC;
        response[5] = P1_DIR_PU;
        response[6] = P3_MOD_OC;
        response[7] = P3_DIR_PU;
        response[8] = PIN_FUNC;
        response[9] = SCON1;
        response[10] = ADC_CFG;
        response[11] = PIN_TDO_IN();
        response[12] = (BOARD_GPIO_PORT(PIN_TDO_PORT) >> PIN_TDO_BIT) & 1U;
        response[13] = DAP_Data.debug_port;
        return DAP_GPIO_DIAG_RESPONSE_SIZE;
    }
#endif
    *response = ID_DAP_Invalid;
    return DAP_COMMAND_ID_SIZE;
}

// Process DAP Vendor extended command request and prepare response
// Default function (can be overridden)
//   request:  pointer to request data
//   response: pointer to response data
//   return:   8 位响应字节数（含命令 ID），零表示容量不足。
//   request:  消耗量由 DAP_Consumed 保存，由组合命令单独推进。
UINT8I DAP_ProcessVendorCommandEx()
{
//    (void)request;
    *response = ID_DAP_Invalid;
    return DAP_COMMAND_ID_SIZE;
}

// Process DAP command request and prepare response
//   request:  pointer to request data
//   response: pointer to response data
//   return:   8 位响应字节数（含命令 ID），零表示容量不足。
//   request:  消耗量由 DAP_Consumed 保存，由组合命令单独推进。
UINT8I DAP_ProcessCommand(const UINT8 xdata * req, UINT8 xdata * resp)
{
    UINT8I request_count = 0U;
    UINT8I response_count = 0U;
    UINT8I header_size;
#if DAP_JTAG
    // 每条命令重建 IR 状态，覆盖原始 SWJ/Pins、配置、断连和 USB reset 后的请求。
    JTAG_InvalidateIR();
#endif
    if (!DAP_RX_AVAILABLE(req, 1U) || !DAP_TX_AVAILABLE(resp, 1U)) { return 0U; }
    DAP_Consumed = DAP_COMMAND_ID_SIZE;
    // 组合命令不允许递归 Execute 或嵌入异步 Abort。
    if (*req == ID_DAP_ExecuteCommands || *req == ID_DAP_TransferAbort) {
        return 0U;
    }
    header_size = 0U;
    if (*req < sizeof(DAP_CommandHeaderSize)) {
        header_size = DAP_CommandHeaderSize[*req];
    } else if (*req == ID_DAP_SWD_Sequence) {
        header_size = 0x11U;
    }
    if (!DAP_RX_AVAILABLE(req + 1U, header_size >> 4) ||
        !DAP_TX_AVAILABLE(resp + 1U, header_size & 0x0FU)) { return 0U; }
    request = req;
    response = resp;

    // 处理供应商特定命令（0x80 ~ 0x9F）
    if ((*request >= ID_DAP_Vendor0) && (*request <= ID_DAP_Vendor31)) {
        return DAP_ProcessVendorCommand();
    }

    // 处理扩展供应商命令（0xA0 ~ 0xFE）
    if ((*request >= ID_DAP_VendorExFirst) && (*request <= ID_DAP_VendorExLast)) {
        return DAP_ProcessVendorCommandEx();
    }

    // 将命令ID写回响应缓冲区首字节
    *response++ = *request;

    // 根据不同的DAP命令调用对应处理函数
    switch (*request++) {
#if SWO_UART
    case ID_DAP_SWO_Transport:
    case ID_DAP_SWO_Mode:
    case ID_DAP_SWO_Baudrate:
    case ID_DAP_SWO_Control:
    case ID_DAP_SWO_Status:
    case ID_DAP_SWO_Data:
    case ID_DAP_SWO_ExtendedStatus:
        header_size = *(request - 1U);
        if (header_size == ID_DAP_SWO_ExtendedStatus) {
            response_count = ((request[0] & 1U) ? 1U : 0U) + ((request[0] & 2U) ? 4U : 0U);
            if (!DAP_TX_AVAILABLE(response, response_count)) { return 0U; }
        }
        response_count = SWO_Command(header_size, request, response, DAP_TX_LEFT(response));
        DAP_Consumed = (DAP_CommandHeaderSize[header_size] >> 4) + DAP_COMMAND_ID_SIZE;
        // ExtendedStatus with no supported fields has a valid empty payload.
        return response_count + DAP_COMMAND_ID_SIZE;
#endif
    case ID_DAP_Info:
        // 处理获取信息命令
        response_count = DAP_Info(*request, response + 1);
        if (response_count == DAP_INFO_OVERFLOW) { return 0U; }
        *response = response_count;                  // 第一个响应字节为信息长度
        DAP_Consumed = DAP_INFO_REQUEST_SIZE + DAP_COMMAND_ID_SIZE;
        return response_count + DAP_INFO_RESPONSE_HEADER_SIZE + DAP_COMMAND_ID_SIZE;
    case ID_DAP_HostStatus:
        // 处理主机状态设置命令
        response_count = DAP_HostStatus(request, response, &request_count);
        break;

    case ID_DAP_Connect:
        // 连接目标设备命令处理
        response_count = DAP_Connect(request, response, &request_count);
        break;
    case ID_DAP_Disconnect:
        // 断开连接命令处理
        response_count = DAP_Disconnect(request, response, &request_count);
        break;

    case ID_DAP_Delay:
        // 处理延时命令
        response_count = DAP_Delay(request, response, &request_count);
        break;

    case ID_DAP_ResetTarget:
        // 目标设备复位命令
        response_count = DAP_ResetTarget(request, response, &request_count);
        break;

    case ID_DAP_SWJ_Pins:
        // SWJ引脚控制命令（SWD/JTAG）
        response_count = DAP_SWJ_Pins(request, response, &request_count);
        break;
    case ID_DAP_SWJ_Clock:
        // 设置SWJ时钟频率命令
        response_count = DAP_SWJ_Clock(request, response, &request_count);
        break;
    case ID_DAP_SWJ_Sequence:
        // SWJ信号序列生成命令
        response_count = DAP_SWJ_Sequence(request, response, &request_count);
        break;

    case ID_DAP_SWD_Configure:
        // 配置SWD模式参数命令
        response_count = DAP_SWD_Configure(request, response, &request_count);
        break;
    case ID_DAP_SWD_Sequence:
        // SWD特定序列命令
        response_count = DAP_SWD_Sequence(request, response, &request_count);
        break;
#if DAP_JTAG
    case ID_DAP_JTAG_Sequence:
        response_count = DAP_JTAG_Sequence(request, response, &request_count);
        break;
    case ID_DAP_JTAG_Configure:
        response_count = DAP_JTAG_Configure(request, response, &request_count);
        break;
    case ID_DAP_JTAG_IDCODE:
        response_count = DAP_JTAG_IDCode(request, response, &request_count);
        break;
#endif

    case ID_DAP_TransferConfigure:
        // 传输参数配置命令
        response_count = DAP_TransferConfigure(request, response, &request_count);
        break;
    case ID_DAP_Transfer:
        // 单次寄存器传输命令处理
        response_count = DAP_Transfer(request, response, &request_count);
        break;
    case ID_DAP_TransferBlock:
        // 块传输命令处理
        response_count = DAP_TransferBlock(request, response, &request_count);
        break;
    case ID_DAP_TransferAbort:
        // Abort pending transfer loop; command has no payload in response
        DAP_TransferAbort_Flag = 1U;
        request_count = 0U;
        response_count = 0U;
        break;

    case ID_DAP_WriteABORT:
        // 写ABORT寄存器命令（用于SWD调试）
        response_count = DAP_WriteAbort(request, response, &request_count);
        break;

    default:
        // 无效命令处理
        *(response - 1) = ID_DAP_Invalid;            // 标记非法命令
        return DAP_COMMAND_ID_SIZE;
    }

    // 8051 命令接口只返回 8 位响应长度，请求推进量独立保存，不打包宽整数。
    if (response_count == 0U) { return 0U; } // 处理函数在容量不足时停止。
    DAP_Consumed = request_count + DAP_COMMAND_ID_SIZE;
    return response_count + DAP_COMMAND_ID_SIZE;
}

// Execute DAP command (process request and prepare response)
//   request:  pointer to request data
//   response: pointer to response data
//   length:   实际 USB OUT 长度；响应缓冲区容量固定为 DAP_PACKET_SIZE。
//   return:   8 位响应字节数（含命令 ID），容量不足返回单字节 Invalid。
UINT8I DAP_ExecuteCommand(const UINT8 xdata * req, UINT8 xdata * resp, UINT8 length)
{
    UINT8I n;
    UINT8I cnt;
    UINT8 xdata *response_head;

    response_head = resp;
    if (length == 0U || length > DAP_PACKET_SIZE) { goto invalid; }
    DAP_RequestEndLow = DAP_POINTER_LOW(req) + length;
    DAP_ResponseEndLow = DAP_POINTER_LOW(resp) + DAP_PACKET_SIZE;

    if (*req == ID_DAP_ExecuteCommands) {
        if (length < 2U) { goto invalid; }
        *resp++ = *req++;
        cnt = *req++;
        *resp++ = cnt;
        while (cnt--) {
            n = DAP_ProcessCommand(req, resp);
            if (n == 0U) { goto invalid; }
            req += DAP_Consumed;
            resp += n;
        }
        return (UINT8I)(resp - response_head);
    }

    n = DAP_ProcessCommand(req, resp);
    if (n != 0U) { return n; }
invalid:
    // 包已满或请求截断时停止，不继续读写；已执行的操作不回滚。
    *response_head = ID_DAP_Invalid;
    return DAP_COMMAND_ID_SIZE;
}

// Setup DAP
void DAP_Setup(void)
{
#if SWO_UART
    SWO_Reset();
#endif
    // Default settings
    DAP_Data.debug_port = 0U;
    DAP_StoreClock(DAP_DEFAULT_SWJ_CLOCK);
#if DAP_SWJ_CALIBRATED
    DAP_Data.clock_delay = (DAP_DEFAULT_SWJ_CLOCK > MAX_SWJ_CLOCK(1U)) ? 0U :
        (UINT16)CH55x_ClockDivide(DAP_DEFAULT_SWJ_CLOCK | CH55X_CLOCK_SWJ_CALIBRATED);
#else
    DAP_Data.clock_delay = CLOCK_DELAY(DAP_DEFAULT_SWJ_CLOCK);
#endif
    DAP_Data.fast_clock = (DAP_Data.clock_delay <= DELAY_FAST_CYCLES) ? 1U : 0U;
    DAP_Data.transfer.idle_cycles = 0U;
    DAP_Data.transfer.retry_count_l = 100U;
    DAP_Data.transfer.retry_count_h = 0U;
    DAP_Data.transfer.match_retry_l = 0U;
    DAP_Data.transfer.match_retry_h = 0U;
    DAP_Data.transfer.match_mask[0] = 0;
    DAP_Data.transfer.match_mask[1] = 0;
    DAP_Data.transfer.match_mask[2] = 0;
    DAP_Data.transfer.match_mask[3] = 0;
    DAP_Data.mode_cfg.swd.turnaround = 1U;
    DAP_Data.mode_cfg.swd.data_phase = 0U;
#if DAP_JTAG
    DAP_Data.mode_cfg.jtag.count = 0U; // Host must supply the real chain.
#endif

//    DAP_SETUP(); // Device specific setup
//    PORT_OFF(); // Avoid influence other device.
}

/******************************************************************************/
/*       Task 1 'dap_task'                                                   */
/******************************************************************************/
void Dap_Task (void) _task_ DAP
{
    UINT8 len;
    UINT8I request_channel;
    UINT8I response_channel;
    UINT8I usb_epoch;
    UINT8I usb_irq;
#if SWO_UART
    UINT8I trace_epoch;
#endif
    DAP_Setup();
#if SWO_UART
    trace_epoch = DAP_UsbEpoch;
#endif
    while (1)
    {
#if SWO_UART
        if (trace_epoch != DAP_UsbEpoch) {
            SWO_Reset();
            trace_epoch = DAP_UsbEpoch;
        }
#endif
        /* endless loop                         */
#if CDC_DAP_YIELD
        // Advisory 8-bit occupancy sample; no queue ownership is changed.
        // Yield before taking request pointers/epoch, then recheck USB state.
        if (DAP_GET_OUT_STATE(DAP_GET_OUT_CH()) && !DAP_GET_IN_STATE(DAP_GET_IN_CH()) &&
            (UINT8)(CDC_RxWrite - CDC_RxRead) >= 128U) {
            os_wait2(K_TMO, 1);
        }
#endif
        if (DAP_GET_OUT_STATE(DAP_GET_OUT_CH()) && !DAP_GET_IN_STATE(DAP_GET_IN_CH()))
        {
            // 固定本次命令的缓冲区，USB ISR 更新其他通道时不清错状态。
            usb_epoch = DAP_UsbEpoch;
            request_channel = DAP_GET_OUT_CH();
            response_channel = DAP_GET_IN_CH();
            len = DAP_ExecuteCommand(UIS_EP1_OUT_BUF(request_channel), UIS_EP1_IN_BUF(response_channel),
                                     DAP_RequestLength[request_channel]);
            // 软件状态的读-改-写与 USB ISR 共用同一字节，提交回复时短暂保护。
            usb_irq = IE_USB;
            IE_USB = 0;
            if (usb_epoch != DAP_UsbEpoch) {
                IE_USB = usb_irq;
                // USB 已重新枚举，不发送上一会话的结果，也不清掉新收到的包。
                DAP_Data.debug_port = DAP_PORT_DISABLED;
                PORT_OFF();
#if SWO_UART
                SWO_Reset();
#endif
                continue;
            }
            DAP_SET_IN_STATE(response_channel, 1);
            DAP_SET_OUT_STATE(request_channel, 0);
#if DAP_PACKET_COUNT == 2
            // 提交后再推进请求队头；IN 仍逐包确认，保持回复顺序。
            if (DAP_GET_OUT_STATE(request_channel ^ 1U)) {
                DAP_SET_OUT_CH(request_channel ^ 1U);
            }
#if DAP_EP1_ATOMIC_CTRL
            // 禁止将整个寄存器的旧值写回：关中断时硬件仍可能翻转同步位。
            // C51 必须生成直接 ANL/ORL，只修改接收响应位。
            if (DAP_GET_OUT_STATE((UEP1_CTRL >> 7) & 1U)) {
                UEP1_CTRL &= ~MASK_UEP_R_RES;
                UEP1_CTRL |= UEP_R_RES_NAK;
            } else {
                UEP1_CTRL &= ~MASK_UEP_R_RES;
            }
#else
            UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_R_RES) |
                (DAP_GET_OUT_STATE((UEP1_CTRL >> 7) & 1U) ? UEP_R_RES_NAK : UEP_R_RES_ACK);
#endif
#endif
            UEP1_T_LEN = len;
            UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
            IE_USB = usb_irq;
        }
        else
        {
            os_wait1(K_SIG);                   /* wait for signal            */
        }
    }
}
