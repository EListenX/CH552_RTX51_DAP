/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 文件名称: SW_DP.C
 * 版本: V2.0.0
 * 日期: 2017年12月1日
 * 项目: CMSIS-DAP
 *
 * 实现 SWJ/SWD 序列和单次 SWD 位传输，包括请求头、ACK、数据、
 * 奇偶校验、turnaround、WAIT/FAULT 数据相位和空闲周期。
 * 报文解析、重试、匹配和 AP posted-read 排空由 DAP.C 负责。
 * 引脚初始化、释放、开漏/高阻和可选 DIR 控制由 IO_Config.H 负责。
 * 未实现目标专属复位；主机通过 DP/AP 访问执行 Cortex-M 软件复位。
 */

#include "DAP.h"
#include "IO_Config.H"
#if DAP_GPIO_SWJ_SWO
#pragma OPTIMIZE(9, SIZE)
#endif
#if DAP_SPI_SWD
#include "DAP_SPI.H"
#endif

#if DAP_SWD_ASM_DATA
extern void SWD_SharedWrite(UINT16 delay, UINT8 value);
extern UINT8 SWD_SharedRead(UINT16 delay);
extern UINT8 SWD_SharedParity(UINT8 value);
#if DAP_SWD_FUSED_PARITY
extern void SWD_SharedWriteParity(UINT8 value);
#endif
extern UINT8 SWD_SharedHeader(UINT8 request);
#if DAP_SWD_FUSED_REQUEST
extern void SWD_SharedRequest(UINT8 request);
#endif
#if DAP_SWD_ASM_ACK
#if DAP_SWD_ACK_WRITE_TURN && !DAP_SWD_ACK_WRITE_ONLY
extern UINT8 SWD_SharedAck(UINT8 turnaround, UINT8 request);
#else
extern UINT8 SWD_SharedAck(UINT8 turnaround);
#endif
#if DAP_SWD_ACK_WRITE_ONLY
extern UINT8 SWD_SharedWriteAck(UINT8 turnaround);
#endif
#endif
#if DAP_SWD_ASM_WORD
#if DAP_SWD_FUSED_READ
extern UINT8 SWD_SharedReadWord(UINT8I *datas, UINT8 turnaround);
#else
extern UINT8 SWD_SharedReadWord(UINT8I *datas);
#endif
#endif
#if DAP_SPI_SWD
/* Keep C calls outside inline ASM leaves: C51 may spill parameters to XRAM,
 * invalidating ACC/DPTR handoff assumptions in their compiler prologues. */
#define SWD_WRITE_BYTE(delay, value) do { \
    if (delay) { SWD_SharedWrite((delay), (value)); } \
    else { DAP_SPI_Write(value); } \
} while (0)
#else
#define SWD_WRITE_BYTE(delay, value) SWD_SharedWrite((delay), (value))
#endif
#endif

// SWCLK 与 TCK 共用引脚，具体端口映射由硬件配置层提供。
#define PIN_SWCLK_SET PIN_SWCLK_TCK_SET    // 设置 SWCLK 为高电平。
#define PIN_SWCLK_CLR PIN_SWCLK_TCK_CLR    // 设置 SWCLK 为低电平。

// 软件延时循环，delay 为循环计数，不保证等于微秒。
// 保留原有 FREQ_SYS 条件路径，不在代码整理中改变已验证的位时序。
// 当前模块未引入 Debug.H；未定义 FREQ_SYS 时每轮只执行一次 SAFE_MOD 操作。
// 实际 SWCLK 频率和采样窗口仍需逻辑分析仪测量。
void PIN_DELAY_SLOW(UINT16 delay)
{
#ifdef FREQ_SYS
#if FREQ_SYS <= 6000000
    delay >>= 2;                  // 低频路径：循环计数除以 4。
#endif
#if FREQ_SYS <= 3000000
    delay >>= 2;                  // 更低频路径：再除以 4。
#endif
#if FREQ_SYS <= 750000
    delay >>= 4;                  // 750 kHz 路径：再除以 16。
#endif
#endif
    while (delay)
    {
        ++SAFE_MOD;              // 保留原有延时操作，不改为其他寄存器访问。
#ifdef FREQ_SYS                   // 显式定义 FREQ_SYS 时保留附加延时操作。
#if FREQ_SYS >= 14000000
        ++SAFE_MOD;              // 第 1 个附加操作。
#endif
#if FREQ_SYS >= 16000000
        ++SAFE_MOD;              // 第 2 个附加操作。
#endif
#if FREQ_SYS >= 18000000
        ++SAFE_MOD;              // 第 3 个附加操作。
#endif
#if FREQ_SYS >= 20000000
        ++SAFE_MOD;              // 第 4 个附加操作。
#endif
#if FREQ_SYS >= 22000000
        ++SAFE_MOD;              // 第 5 个附加操作。
#endif
#if FREQ_SYS >= 24000000
        ++SAFE_MOD;              // 第 6 个附加操作。
#endif
#endif
        --delay;
    }
}

#define PIN_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)

#define SW_CLOCK_CYCLE() \
    PIN_SWCLK_CLR();     \
    PIN_DELAY();         \
    PIN_SWCLK_SET();     \
    PIN_DELAY()

#define SW_WRITE_BIT(bits) \
    PIN_SWDIO_OUT(bits);   \
    PIN_SWCLK_CLR();      \
    PIN_DELAY();          \
    PIN_SWCLK_SET();      \
    PIN_DELAY()

#define SW_READ_BIT(bits)  \
    PIN_SWCLK_CLR();      \
    PIN_DELAY();          \
    bits = PIN_SWDIO_IN(); \
    PIN_SWCLK_SET();      \
    PIN_DELAY()

// Generate SWJ Sequence
//   count:  sequence bit count
//   data:   pointer to sequence bit data
//   return: none
#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
void SWJ_Sequence(UINT16I count, const UINT8 xdata *datas)
{
    UINT8I val = 0U;
    UINT8I n = 0U;

#if DAP_JTAG
    if (DAP_Data.debug_port != DAP_PORT_JTAG)
#endif
    { PIN_SWDIO_OUT_ENABLE(); }
    while (count--) {
        if (n == 0U) {
            val = *datas++;
            n = 8U;
        }
        PIN_SWDIO_OUT(val);
        SW_CLOCK_CYCLE();
        val >>= 1;
        n--;
    }
}
#endif



#if (DAP_SWD != 0)
void SWD_Sequence(UINT8I info, const UINT8 xdata *swdo, UINT8 xdata *swdi)
{
    UINT8I val;
    UINT8I bits;
    UINT8I n, k;

    n = info & SWD_SEQUENCE_CLK;
    if (n == 0U) {
        n = 64U;
    }

    if (info & SWD_SEQUENCE_DIN) {
        // DIN：释放 SWDIO，由目标驱动；按 LSB-first 采样。
        PIN_SWDIO_OUT_DISABLE();
        while (n) {
            val = 0U;
            for (k = 8U; k && n; k--, n--) {
                SW_READ_BIT(bits);
                val >>= 1;
                val |= bits << 7;
            }
            val >>= k;
            *swdi++ = (UINT8I)val;
        }
    } else {
        // DOUT：由探针驱动 SWDIO，按 LSB-first 输出请求数据。
        PIN_SWDIO_OUT_ENABLE();
        while (n) {
            val = *swdo++;
            for (k = 8U; k && n; k--, n--) {
                SW_WRITE_BIT(val);
                val >>= 1;
            }
        }
    }
}


// 单一 SWD 状态机；每次传输固定时钟模式，慢档才执行延时。
#undef PIN_DELAY
#define PIN_DELAY() do { if (slow_clock) { PIN_DELAY_SLOW(DAP_Data.clock_delay); } } while (0)
/* BEGIN SWD_Transfer */
/* Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared byte-oriented protocol path; PIN_DELAY selects the clock timing.
 */
UINT8 SWD_Transfer(UINT8I request, UINT8I * datas) DAP_SWD_MEMORY
{
#if DAP_SWD_SMALL_CORE
    UINT8 ack;
    UINT8 bits;
    UINT8 val;
    UINT8 parity;
    UINT8 n, b;
#else
    UINT8I ack;
    UINT8I bits;
    UINT8I val;
    UINT8I parity;


    UINT8I n, b;
#endif
    bit slow_clock;
#if DAP_SWD_ASM_DATA
#if DAP_SWD_SMALL_CORE
    UINT16 data_delay;
#else
    UINT16I data_delay;
#endif
#endif

    slow_clock = !DAP_Data.fast_clock;
#if DAP_SPI_SWD
    DAP_SPI_Failed = 0;
#endif
#if DAP_SWD_ASM_DATA
    data_delay = slow_clock ? DAP_Data.clock_delay : 0U;
#endif

    /* 请求包固定包含 Start/APnDP/RnW/A2/A3/Parity/Stop/Park。 */
    PIN_SWDIO_OUT_ENABLE();
    /* Packet Request */
#if DAP_SWD_ASM_DATA && !DAP_SWD_DIAG_C_WRITE
    /* 共享汇编按字节发头部，不使用 hdr1 的逐位 C 循环。 */
#if DAP_SWD_FUSED_REQUEST
    if (!slow_clock) {
        SWD_SharedRequest(request);
    } else {
#endif
    val = SWD_SharedHeader(request);
#if DAP_SPI_SWD_GPIO_REQUEST
    /* 仅请求头回到GPIO；WDATA仍走SPI，慢档延时保持原语义。 */
    SWD_SharedWrite(data_delay, val);
#else
    SWD_WRITE_BYTE(data_delay, val);
#endif
#if DAP_SWD_FUSED_REQUEST
    }
#endif
#if DAP_SPI_SWD
    if (DAP_SPI_Failed) { goto spi_error; }
#endif
#else
    parity = 0U;
    SW_WRITE_BIT(1U); /* Start Bit */
    bits = request >> 0;
    SW_WRITE_BIT(bits); /* APnDP Bit */
    parity += bits;
    bits = request >> 1;
    SW_WRITE_BIT(bits); /* RnW Bit */
    parity += bits;
    bits = request >> 2;
    SW_WRITE_BIT(bits); /* A2 Bit */
    parity += bits;
    bits = request >> 3;
    SW_WRITE_BIT(bits); /* A3 Bit */
    parity += bits;
    SW_WRITE_BIT(parity); /* Parity Bit */
    SW_WRITE_BIT(0U); /* Stop Bit */
    SW_WRITE_BIT(1U); /* Park Bit */
#endif

    /* Turnaround */
    PIN_SWDIO_OUT_DISABLE();
#if DAP_SWD_ASM_ACK
    if (!slow_clock) {
#if DAP_SWD_ACK_WRITE_ONLY
        if (request & DAP_TRANSFER_RnW) {
            ack = SWD_SharedAck(DAP_Data.mode_cfg.swd.turnaround);
        } else {
            ack = SWD_SharedWriteAck(DAP_Data.mode_cfg.swd.turnaround);
        }
#elif DAP_SWD_ACK_WRITE_TURN
        ack = SWD_SharedAck(DAP_Data.mode_cfg.swd.turnaround, request);
#else
        ack = SWD_SharedAck(DAP_Data.mode_cfg.swd.turnaround);
#endif
    } else
#endif
    {
    for (n = DAP_Data.mode_cfg.swd.turnaround; n; n--) {
        SW_CLOCK_CYCLE();
    }

    /* Acknowledge response */
    SW_READ_BIT(bits);
    ack = bits << 0;
    SW_READ_BIT(bits);
    ack |= bits << 1;
    SW_READ_BIT(bits);
    ack |= bits << 2;
    }

    if (ack == DAP_TRANSFER_OK) { /* OK response */
        /* Data transfer */
        if (request & DAP_TRANSFER_RnW) {
            /* Read data */
            parity = 0U;
#if DAP_SWD_ASM_WORD
            if (!slow_clock) {
#if DAP_SWD_FUSED_READ
                parity = SWD_SharedReadWord(datas, DAP_Data.mode_cfg.swd.turnaround);
                if (parity) { ack = DAP_TRANSFER_ERROR; }
#else
                parity = SWD_SharedReadWord(datas);
#endif
            } else
#endif
            {
            // 每次只组装一个字节；NULL 表示丢弃 posted-read 数据，仍接收完整数据相位。
            for (b = 0; b < 4; b++) {
#if DAP_SWD_ASM_DATA && !DAP_SWD_DIAG_C_READ
                val = SWD_SharedRead(data_delay);
                parity ^= val; // 字节异或后取奇偶性，NULL 仍接收完整数据相位。
#else
                val = 0U;
                for (n = 8; n; n--) {
                    SW_READ_BIT(bits); /* Read RDATA[0:31] */
                    parity += bits;
                    val >>= 1;
                    val |= bits << 7;
                }
#endif
                if (datas) {
                    datas[b] = val;
                }
            }
#if DAP_SWD_ASM_DATA && !DAP_SWD_DIAG_C_READ
            parity = SWD_SharedParity(parity);
#endif
#if !DAP_SWD_FUSED_READ
            }
#endif
            SW_READ_BIT(bits); /* Read Parity */
            if ((parity ^ bits) & 1U) {
                ack = DAP_TRANSFER_ERROR;
            }
            /* Turnaround */
            for (n = DAP_Data.mode_cfg.swd.turnaround; n; n--) {
                SW_CLOCK_CYCLE();
            }
#if DAP_SWD_FUSED_READ
            }
#endif
            PIN_SWDIO_OUT_ENABLE();
        } else {
            /* Turnaround */
#if DAP_SWD_ACK_WRITE_TURN
            // 快档 OK 写响应已在 ACK 叶内消费换向；输出方向仍由板级接口恢复。
            if (slow_clock)
#endif
            {
            for (n = DAP_Data.mode_cfg.swd.turnaround; n; n--) {
                SW_CLOCK_CYCLE();
            }
            }
            PIN_SWDIO_OUT_ENABLE();
            /* Write data */
            parity = 0U;
#if DAP_SPI_SWD
            if (!slow_clock) {
                parity = DAP_SPI_WriteWord(datas);
                if (DAP_SPI_Failed) { goto spi_error; }
#if DAP_SWD_FUSED_PARITY
                SWD_SharedWriteParity(parity);
#endif
            } else
#endif
            {
            for (b = 0; b < 4; b++) {
                val = *(datas + b);
#if DAP_SWD_ASM_DATA && !DAP_SWD_DIAG_C_WRITE
                parity ^= val;
                SWD_WRITE_BYTE(data_delay, val);
#if DAP_SPI_SWD
                if (DAP_SPI_Failed) { goto spi_error; }
#endif
#else
                for (n = 8U; n; n--) {
                    SW_WRITE_BIT(val); /* Write WDATA[0:31] */
                    parity += val;
                    val >>= 1;
                }
#endif
            }
#if !DAP_SWD_FUSED_PARITY
            }
#endif
#if DAP_SWD_ASM_DATA && !DAP_SWD_DIAG_C_WRITE
            parity = SWD_SharedParity(parity);
#endif
            SW_WRITE_BIT(parity); /* Write Parity Bit */
#if DAP_SWD_FUSED_PARITY
            }
#endif
        }
        /* Capture Timestamp */
        if (request & DAP_TRANSFER_TIMESTAMP) {
            DAP_Data.timestamp = TIMESTAMP_GET();
        }
        /* Idle cycles */
        if (DAP_Data.transfer.idle_cycles) {
            PIN_SWDIO_OUT(0U);
            for (b = DAP_Data.transfer.idle_cycles; b; b--) {
                SW_CLOCK_CYCLE();
            }
        }
        PIN_SWDIO_OUT(1U);
        return ((UINT8)ack);
    }

    if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
        /* WAIT or FAULT response */
        if (DAP_Data.mode_cfg.swd.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) {
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE(); /* Dummy Read RDATA[0:31] + Parity */
            }
        }
        /* Turnaround */
        for (n = DAP_Data.mode_cfg.swd.turnaround; n; n--) {
            SW_CLOCK_CYCLE();
        }
        PIN_SWDIO_OUT_ENABLE();
        if (DAP_Data.mode_cfg.swd.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) {
            PIN_SWDIO_OUT(0U);
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE(); /* Dummy Write WDATA[0:31] + Parity */
            }
        }
        PIN_SWDIO_OUT(1U);
        return ((UINT8)ack);
    }

    /* Protocol error */
    for (n = DAP_Data.mode_cfg.swd.turnaround + 32U + 1U; n; n--) {
        SW_CLOCK_CYCLE(); /* Back off data phase */
    }
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);
    return DAP_TRANSFER_ERROR;
#if DAP_SPI_SWD
spi_error:
    PIN_SWDIO_OUT_DISABLE();
    PIN_SWCLK_SET();
    return DAP_TRANSFER_ERROR;
#endif
}
/* END SWD_Transfer */

#endif
