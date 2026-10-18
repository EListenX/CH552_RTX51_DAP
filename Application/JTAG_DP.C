/* Copyright (c) 2013-2017 ARM Limited. SPDX-License-Identifier: Apache-2.0
 * Byte-oriented GPIO JTAG, based on CMSIS-DAP JTAG_DP.c.
 * TAP navigation belongs here; packet parsing/WAIT budget belongs to DAP.C.
 */
#include "DAP.H"
#include "IO_Config.H"
#if DAP_SPI_JTAG
#include "DAP_SPI.H"
#endif
#if DAP_JTAG_GPIO_TIGHT
#pragma OPTIMIZE(9, SIZE)
#else
#pragma OPTIMIZE(8, SIZE)
#endif

#if DAP_JTAG
extern void PIN_DELAY_SLOW(UINT16 delay);
#if DAP_SPI_JTAG || DAP_JTAG_GPIO_CORE
#define JTAG_MEMORY small
#else
#define JTAG_MEMORY
#endif
#if DAP_SPI_JTAG_WORD || DAP_JTAG_GPIO_CORE
#define JTAG_AUX_MEMORY small
#else
#define JTAG_AUX_MEMORY
#endif

/* 时钟配置只由 DAP 任务修改；每命令刷新，避免每个 GPIO 半周期读 XDATA。 */
#if DAP_JTAG_FAST_CACHE
static bit jtag_fast_clock;
#define JTAG_FAST_CLOCK jtag_fast_clock
#else
#define JTAG_FAST_CLOCK DAP_Data.fast_clock
#endif

/* 仅在一条 DAP 命令内复用 IR；不跨外部 Sequence/复位推测 TAP 状态。 */
#if DAP_SPI_JTAG_STREAM || DAP_JTAG_GPIO_CORE
static UINT8 data jtag_ir_key;
#else
static UINT8 idata jtag_ir_key;
#endif

void JTAG_InvalidateIR(void)
{
    jtag_ir_key = 0U; // 实现的 IR 均非零，零表示缓存无效。
#if DAP_JTAG_FAST_CACHE
    jtag_fast_clock = DAP_Data.fast_clock;
#endif
#if DAP_SPI_JTAG
    DAP_SPI_Failed = 0;
#endif
}

/* All GPIO goes through the board layer. No interrupt masking or wide shifts. */
#if DAP_JTAG_BIT_CLOCK
/* TDO 本来只有一位，C51 用 CY 返回，避免调用点反复装拆 8 位布尔值。
 * slow delay 会破坏 CY，采样仍保存到任务私有位，第二次延时后才返回。
 * static 函数及调用点一起编译，不改变任何外部函数原型或汇编入口。
 */
static bit JTAG_Clock(UINT8I tms, UINT8I tdi) JTAG_MEMORY
#else
static UINT8I JTAG_Clock(UINT8I tms, UINT8I tdi) JTAG_MEMORY
#endif
{
#if DAP_JTAG_BIT_CLOCK
    bit sampled;
#elif DAP_SPI_JTAG_STREAM
    UINT8 sampled; // small 叶函数使用 DATA，避免采样暂存的间接寻址。
#else
    UINT8I sampled;
#endif
    PIN_SWDIO_OUT(tms);
    PIN_TDI_OUT(tdi);
    PIN_SWCLK_TCK_CLR();
    if (!JTAG_FAST_CLOCK) { PIN_DELAY_SLOW(DAP_Data.clock_delay); }
    sampled = PIN_TDO_IN();
    PIN_SWCLK_TCK_SET();
    if (!JTAG_FAST_CLOCK) { PIN_DELAY_SLOW(DAP_Data.clock_delay); }
    return sampled;
}

#if DAP_JTAG_GPIO_BYTE
/* Internal count=1..8; TMS=0/255 is constant, 128 exits on bit 8 (DR only).
 * One mode test per byte. Partial captures return right-aligned with zero high bits.
 * No SPI, IRQ masking or shared scratch; slow mode keeps both original delays.
 */
static UINT8 JTAG_GPIOByte(UINT8 value, UINT8 count, UINT8 tms) small
{
    UINT8 n;
    bit sampled;
    n = count;
    if (JTAG_FAST_CLOCK) {
        PIN_SWDIO_OUT(tms);
#define JTAG_GPIO_STEP() do { \
    PIN_TDI_OUT(value); PIN_SWCLK_TCK_CLR(); \
    sampled = PIN_TDO_IN(); PIN_SWCLK_TCK_SET(); \
    value >>= 1; if (sampled) { value |= 128U; } \
} while (0)
        while (--n) { JTAG_GPIO_STEP(); }
        if (tms == 128U) { PIN_SWDIO_OUT(1U); }
        JTAG_GPIO_STEP();
#undef JTAG_GPIO_STEP
    } else {
        do {
            sampled = JTAG_Clock(tms, value);
            value >>= 1;
            if (sampled) { value |= 128U; }
            tms >>= 1;
        } while (--n);
    }
#if DAP_JTAG_GPIO_TIGHT
/* Complete bytes are already aligned; only partial Sequence bytes need shifting. */
    if (count == 8U) { return value; }
#endif
    return value >> (8U - count);
}
#endif

#if DAP_JTAG_TAP_PATH
/* 固定导航只写 TMS，TDI 恒为 1；不采样已丢弃的 TDO，不接触 SPI。
 * 内部调用保证 count 为 2..4，LSB-first；快档省逐位调用，慢档仍用原延时。
 * 调用时 SPI 已归还引脚，TCK 为高；结束同样保持高电平。
 */
static void JTAG_Path(UINT8 tms, UINT8 count) small
{
    PIN_TDI_OUT(1U);
#if DAP_JTAG_GPIO_TIGHT
    /* The DAP task owns clock mode; it cannot change during this path. */
    if (JTAG_FAST_CLOCK) {
        do {
            PIN_SWDIO_OUT(tms);
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
            tms >>= 1;
        } while (--count);
        return;
    }
    do {
        JTAG_Clock(tms, 1U);
        tms >>= 1;
    } while (--count);
#else
    do {
        if (JTAG_FAST_CLOCK) {
            PIN_SWDIO_OUT(tms);
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
        } else {
            JTAG_Clock(tms, 1U);
        }
        tms >>= 1;
    } while (--count);
#endif
}
#endif

#if DAP_SPI_JTAG
/* tms is a bit mask, so an exit on the eighth bit stays on GPIO. */
static UINT8 JTAG_ShiftByte(UINT8 value, UINT8 count, UINT8 tms) small
{
    UINT8 output, mask;
    /* Callers supply only 0, 255, or 128 (last-bit exit). */
    if (JTAG_FAST_CLOCK && count == 8U && tms != 128U) {
#if !DAP_SPI_JTAG_STREAM
        PIN_SWDIO_OUT(tms);
#endif
        return DAP_SPI_Exchange(value);
    }
#if DAP_SPI_JTAG_STREAM
    /* 只在数据段转入 GPIO 时归还，不给每个 GPIO 位增加寄存器写。 */
    SPI0_CTRL = bS0_MST_CLK;
#endif
    output = 0U;
    mask = 1U;
    while (count--) {
        if (JTAG_Clock(tms, value)) { output |= mask; }
        mask <<= 1;
        value >>= 1;
        tms >>= 1;
    }
    return output;
}
#endif

void JTAG_Sequence(UINT8I info, const UINT8 xdata *tdi, UINT8 xdata *tdo) DAP_JTAG_MEMORY
{
#if DAP_SPI_JTAG_STREAM || DAP_JTAG_GPIO_CORE
    UINT8 n, k, input, output; // small 热循环用 DATA，避免逐字节 IDATA 间接访问。
#else
    UINT8I n, k, input, output;
#endif
    JTAG_InvalidateIR();
#if DAP_SPI_JTAG_STREAM
    /* TMS 整段固定，只设置一次；DR 数据段已由 DRBegin 置零。 */
    PIN_SWDIO_OUT(info >> 6);
#endif
    n = info & JTAG_SEQUENCE_TCK;
    if (!n) { n = 64U; }
    while (n) {
        input = *tdi++;
        output = 0U;
#if DAP_SPI_JTAG
        k = n >= 8U ? 8U : n;
        output = JTAG_ShiftByte(input, k, info & JTAG_SEQUENCE_TMS ? 255U : 0U);
        if (DAP_SPI_Failed) { return; }
        n -= k;
#elif DAP_JTAG_GPIO_BYTE
        k = n >= 8U ? 8U : n;
        output = JTAG_GPIOByte(input, k, info & JTAG_SEQUENCE_TMS ? 255U : 0U);
        n -= k;
#else
        for (k = 8U; k && n; k--, n--) {
            output >>= 1;
#if DAP_JTAG_BIT_CLOCK
            output |= (UINT8)JTAG_Clock(info >> 6, input) << 7;
#else
            output |= JTAG_Clock(info >> 6, input) << 7;
#endif
            input >>= 1;
        }
        output >>= k;
#endif
        if (info & JTAG_SEQUENCE_TDO) { *tdo++ = output; }
    }
#if DAP_SPI_JTAG_STREAM
    /* 整字节段也必须在返回 DAP 分发器前归还引脚。 */
    SPI0_CTRL = bS0_MST_CLK;
#endif
}

/* Caller starts in Run-Test/Idle. All non-selected TAPs receive BYPASS. */
static void JTAG_IR(UINT8I instruction) JTAG_MEMORY
{
#if DAP_JTAG_IR_CACHE
    UINT8 count, selected; // 链配置仅 DAP 任务修改；每次 IR 调用重新快照。
#endif
#if DAP_SPI_JTAG_WORD || DAP_JTAG_GPIO_CORE
    UINT8 device, bit_count, value, key;
#else
    UINT8I device, bit_count, value, key;
#endif
    // 最多 8 个 TAP；低四位保存 IR，高四位保存索引，避免宽整数打包。
#if DAP_JTAG_IR_CACHE
    selected = DAP_Data.mode_cfg.jtag.index;
    key = instruction | (selected << 4);
#else
    key = instruction | (DAP_Data.mode_cfg.jtag.index << 4);
#endif
    if (jtag_ir_key == key) { return; }
#if DAP_JTAG_IR_CACHE
    count = DAP_Data.mode_cfg.jtag.count; // 命中 IR 缓存时不额外读取链长度。
#endif
#if DAP_JTAG_TAP_PATH
    JTAG_Path(3U, 4U); // Idle -> Shift-IR，TMS 为 1,1,0,0。
#else
    JTAG_Clock(1U, 1U); JTAG_Clock(1U, 1U);
    JTAG_Clock(0U, 1U); JTAG_Clock(0U, 1U);
#endif
#if DAP_JTAG_IR_CACHE
    for (device = 0U; device < count; device++) {
#else
    for (device = 0U; device < DAP_Data.mode_cfg.jtag.count; device++) {
#endif
        value = instruction;
#if DAP_JTAG_FAST_CACHE
        /* 每个 TAP 只取一次 IR 长度；倒计数避免每位读取 XDATA 数组。 */
        for (bit_count = DAP_Data.mode_cfg.jtag.ir_length[device]; bit_count; bit_count--) {
#if DAP_JTAG_IR_CACHE
            JTAG_Clock((device == count - 1U) && (bit_count == 1U),
                       device == selected ? value : 1U);
#else
            JTAG_Clock((device == DAP_Data.mode_cfg.jtag.count - 1U) && (bit_count == 1U),
                       device == DAP_Data.mode_cfg.jtag.index ? value : 1U);
#endif
            value >>= 1;
        }
#else
        for (bit_count = 0U; bit_count < DAP_Data.mode_cfg.jtag.ir_length[device]; bit_count++) {
            JTAG_Clock((device == DAP_Data.mode_cfg.jtag.count - 1U) &&
                       (bit_count == DAP_Data.mode_cfg.jtag.ir_length[device] - 1U),
                       device == DAP_Data.mode_cfg.jtag.index ? value : 1U);
            value >>= 1;
        }
#endif
    }
#if DAP_JTAG_TAP_PATH
    JTAG_Path(1U, 2U); // Exit1-IR -> Idle，TMS 为 1,0。
#else
    JTAG_Clock(1U, 1U); JTAG_Clock(0U, 1U);
#endif
    jtag_ir_key = key;
}

static void JTAG_DRBegin(void) JTAG_AUX_MEMORY
{
#if DAP_SPI_JTAG_WORD || DAP_JTAG_GPIO_CORE
    UINT8 n;
#else
    UINT8I n;
#endif
#if DAP_JTAG_TAP_PATH
    JTAG_Path(1U, 3U); // Idle -> Shift-DR，TMS 为 1,0,0。
#else
    JTAG_Clock(1U, 1U); JTAG_Clock(0U, 1U); JTAG_Clock(0U, 1U);
#endif
    for (n = DAP_Data.mode_cfg.jtag.index; n; n--) { JTAG_Clock(0U, 1U); }
}

static void JTAG_DREnd(void) JTAG_AUX_MEMORY
{
#if DAP_JTAG_TAP_PATH
    JTAG_Path(1U, 2U); // Exit1-DR -> Idle，TMS 为 1,0。
#else
    JTAG_Clock(1U, 1U); JTAG_Clock(0U, 1U);
#endif
}

/* Scan 32 data bits, appending one BYPASS bit per later TAP. */
static void JTAG_Word(UINT8I read, UINT8I *datas) JTAG_MEMORY
{
#if DAP_SPI_JTAG_STREAM
    UINT8 b, value, remaining;
#elif DAP_SPI_JTAG
    UINT8I b, value, remaining;
#elif DAP_JTAG_GPIO_CORE
#if DAP_JTAG_GPIO_BYTE
    UINT8 b, value, remaining;
#else
    UINT8 b, n, value, sampled, remaining;
#endif
#else
    UINT8I b, n, value, sampled, remaining;
#endif
#if DAP_SPI_JTAG_WORD && !DAP_JTAG_ASM_FIFO
    UINT8 timeout;
#endif
#if DAP_SPI_JTAG_WORD
    if (DAP_SPI_Failed) { return; }
#endif
    // 数据和后置 BYPASS 共用倒计数，逐位循环不再反复比较字节/位索引。
    remaining = 31U + DAP_Data.mode_cfg.jtag.count - DAP_Data.mode_cfg.jtag.index;
    for (b = 0U; b < 4U; b++) {
        value = (!read && datas) ? datas[b] : 0U;
#if DAP_SPI_JTAG
#if DAP_SPI_JTAG_WORD
        if (JTAG_FAST_CLOCK && remaining != 8U) {
#if DAP_JTAG_ASM_FIFO
#if DAP_JTAG_ASM_ACTIVE
            /* 首字节接管，后续复用同一汇编叶；逐字节检查失败后才可继续。 */
            if (!b) { SPI0_CTRL = bS0_MST_CLK | bS0_SCK_OE | bS0_MOSI_OE; }
            value = DAP_SPI_ExchangeActive(value);
#else
            // Sequence/DR 共用寄存器叶，保持相同 FIFO 与超时边界。
            value = DAP_SPI_Exchange(value);
#endif
#else
            /* TMS 已由 DRBegin 置低；整个 DR 数据段只接管一次。
             * FREE 后先消费 RX 才能再写 FIFO；不经过逐字节调用链。
             */
            if (!b) { SPI0_CTRL = bS0_MST_CLK | bS0_SCK_OE | bS0_MOSI_OE; }
            SPI0_DATA = value;
            timeout = 0U;
            while (!S0_FREE) {
                if (--timeout == 0U) {
                    DAP_SPI_Failed = 1;
                    break;
                }
            }
            /* 完成/超时均保留最后实际发送值，归还引脚时不改变 TDI。 */
            PIN_TDI_OUT(value >> 7);
            if (DAP_SPI_Failed) {
                SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL;
                SPI0_CTRL = bS0_MST_CLK;
                return;
            }
            value = SPI0_DATA;
#endif
        } else {
            value = JTAG_ShiftByte(value, 8U, remaining == 8U ? 128U : 0U);
        }
#else
        /* Keep the final Shift-DR exit clock on GPIO, including a one-TAP chain. */
        value = JTAG_ShiftByte(value, 8U, remaining == 8U ? 128U : 0U);
#endif
        if (DAP_SPI_Failed) { return; }
        remaining -= 8U;
#elif DAP_JTAG_GPIO_BYTE
        value = JTAG_GPIOByte(value, 8U, remaining == 8U ? 128U : 0U);
        remaining -= 8U;
#else
        for (n = 0U; n < 8U; n++) {
            remaining--;
            sampled = JTAG_Clock(remaining == 0U, value);
            value >>= 1;
            if (read) { value |= sampled << 7; }
        }
#endif
        if (read && datas) { datas[b] = value; }
    }
#if DAP_SPI_JTAG_STREAM
    /* 后置 BYPASS 和 TAP 退出属于 GPIO；先归还 SPI 的 TCK/TDI。 */
    SPI0_CTRL = bS0_MST_CLK;
#endif
    while (remaining) { JTAG_Clock(remaining == 1U, 1U); remaining--; }
    JTAG_DREnd();
}

void JTAG_ReadIDCode(UINT8I *datas) DAP_JTAG_MEMORY
{
    JTAG_IR(JTAG_IDCODE);
    JTAG_DRBegin();
    JTAG_Word(1U, datas);
}

static UINT8I JTAG_Scan(UINT8I request_value, UINT8I *datas, UINT8I abort_write) JTAG_MEMORY
{
#if DAP_SPI_JTAG_WORD || DAP_JTAG_GPIO_CORE
    UINT8 ack, n;
#else
    UINT8I ack, n;
#endif
    JTAG_DRBegin();
    /* JTAG wire ACK 010=OK/FAULT, 001=WAIT; swap bit0/bit1 for CMSIS-DAP. */
#if DAP_JTAG_BIT_CLOCK
    ack = (UINT8)JTAG_Clock(0U, request_value >> 1) << 1;
    ack |= JTAG_Clock(0U, request_value >> 2);
    ack |= (UINT8)JTAG_Clock(0U, request_value >> 3) << 2;
#else
    ack = JTAG_Clock(0U, request_value >> 1) << 1;
    ack |= JTAG_Clock(0U, request_value >> 2);
    ack |= JTAG_Clock(0U, request_value >> 3) << 2;
#endif
    if (ack == DAP_TRANSFER_OK || abort_write) {
        JTAG_Word(request_value & DAP_TRANSFER_RnW, datas);
#if DAP_SPI_JTAG
        if (DAP_SPI_Failed) { return DAP_TRANSFER_ERROR; }
#endif
    } else {
        JTAG_Clock(1U, 1U); // Exit1-DR; WAIT does not accept new transaction.
        JTAG_DREnd();
    }
    for (n = DAP_Data.transfer.idle_cycles; n; n--) { JTAG_Clock(0U, 1U); }
    if (ack != DAP_TRANSFER_OK && ack != DAP_TRANSFER_WAIT) { ack = DAP_TRANSFER_ERROR; }
    return ack;
}

/* Adapt JTAG's delayed DP reads to the existing SWD-style byte parser.
 * AP reads remain posted. RDBUFF drains the previously posted DP/AP result.
 * Every other DP read is primed and drained locally (extra scan, no 32-bit temp).
 */
UINT8I JTAG_Transfer(UINT8I request_value, UINT8I *datas) DAP_JTAG_MEMORY
{
#if DAP_SPI_JTAG_WORD || DAP_JTAG_GPIO_CORE
    UINT8 ack;
#else
    UINT8I ack;
#endif
    JTAG_IR(request_value & DAP_TRANSFER_APnDP ? JTAG_APACC : JTAG_DPACC);
    /* Address 4/8 DP reads also need priming; only address 12 is a drain. */
    if (!(request_value & DAP_TRANSFER_APnDP) && (request_value & DAP_TRANSFER_RnW) &&
        (request_value & DP_RDBUFF) != DP_RDBUFF) {
        ack = JTAG_Scan(request_value, NULL, 0U);
        if (ack != DAP_TRANSFER_OK) { return ack; }
        return JTAG_Scan(DP_RDBUFF | DAP_TRANSFER_RnW, datas, 0U);
    }
    return JTAG_Scan(request_value, datas, 0U);
}

void JTAG_WriteAbort(UINT8I *datas) DAP_JTAG_MEMORY
{
    JTAG_IR(JTAG_ABORT);
    JTAG_Scan(0U, datas, 1U); // ABORT scan has no ACK status, per Arm.
}
#endif
