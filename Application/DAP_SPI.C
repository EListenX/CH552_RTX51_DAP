/* SPI0 supports exclusive SWD write or JTAG full-duplex byte profiles.
 * SWD reads/ACK/turnaround and JTAG state changes use GPIO.
 * Initial conservative divider is a bench candidate, not a measured clock.
 * SPI0 belongs exclusively to DAP task; no ISR uses it or masks interrupts.
 */
#include "DAP_SPI.H"
#include "IO_Config.H"
#if DAP_SPI_JTAG
#pragma OPTIMIZE(8, SIZE)
#endif
#if DAP_SPI_SWD || DAP_SPI_JTAG
bit DAP_SPI_Failed;

/* Configure once per port setup, never in the byte transmission loop. */
void DAP_SPI_Init(void)
{
    SPI0_SETUP = bS0_BIT_ORDER;
    SPI0_CK_SE = DAP_SPI_DIV;
    SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL;
    SPI0_CTRL = bS0_MST_CLK;
    DAP_SPI_Failed = 0;
}

/* One active byte at a time; FREE means the shift engine has finished.
 * Inline the bounded poll: avoid a call/return for each 8-bit transfer.
 * Counter is local to the calling task, no shared scratch or IRQ masking. */
#define SPI_WAIT(spi_timeout_) do { \
    spi_timeout_ = 0U; \
    while (!S0_FREE) { \
        if (--spi_timeout_ == 0U) { DAP_SPI_Failed = 1; break; } \
    } \
} while (0)

#if DAP_SPI_JTAG
/* Three-wire Mode3: write launches exactly eight clocks, read does not.
 * MISO stays input. GPIO latches match the final SPI output before release.
 */
#if !DAP_JTAG_ASM_FIFO
UINT8 DAP_SPI_Exchange(UINT8 value) small
{
    UINT8 received, timeout;
    if (DAP_SPI_Failed) { return 0U; }
    SPI0_CTRL = bS0_MST_CLK | bS0_SCK_OE | bS0_MOSI_OE;
    SPI0_DATA = value;
    SPI_WAIT(timeout);
    received = 0U;
    if (!DAP_SPI_Failed) { received = SPI0_DATA; }
    PIN_TDI_OUT(value >> 7);
#if DAP_SPI_JTAG_STREAM
    /* 连续字节保持 SPI 所有权；GPIO 时钟或 Sequence 结束时再归还。
     * 每字节仍消费 RX FIFO，并预置末位 GPIO，切换时不改变 TDI。
     * 超时立即停止并归还，不让后续字节继承未完成传输。
     */
    if (DAP_SPI_Failed) {
        SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL;
        SPI0_CTRL = bS0_MST_CLK;
    }
#else
    if (DAP_SPI_Failed) { SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL; }
    SPI0_CTRL = bS0_MST_CLK;
#endif
    return received;
}
#endif
#endif

#if DAP_SPI_SWD
#if DAP_SPI_SWD_2WIRE
/* 两线写相位仅驱动MISO，MOSI不接管。DATA_DIR=0只允许写FIFO启动。
 * 结束时先预置GPIO末位，再关闭SPI输出；ACK/读/换向沿用GPIO。
 */
#define SPI_SWD_WRITE_CTRL (bS0_MST_CLK | bS0_2_WIRE | bS0_SCK_OE | bS0_MISO_OE)
#else
#define SPI_SWD_WRITE_CTRL (bS0_MST_CLK | bS0_SCK_OE | bS0_MOSI_OE)
#endif
#define SPI_BEGIN() do { \
    P1_MOD_OC &= ~PIN_SWDIO_MASK; \
    SPI0_CTRL = SPI_SWD_WRITE_CTRL; \
} while (0)

static void DAP_SPI_End(UINT8 value)
{
    /* Prime GPIO latches while peripheral still owns pins, then hand back.
     * Abort clears a possibly unfinished shift before releasing ownership.
     */
    /* Caller left the GPIO clock latch high; Mode3 also ends high. */
    PIN_SWDIO_OUT(value >> 7);
    if (DAP_SPI_Failed) { SPI0_CTRL = bS0_MST_CLK | bS0_CLR_ALL; }
    SPI0_CTRL = bS0_MST_CLK;
    BOARD_SWDIO_DRIVE_MODE();
}

void DAP_SPI_Write(UINT8 value)
{
    UINT8 timeout;
    if (DAP_SPI_Failed) { return; }
    SPI_BEGIN();
    SPI0_DATA = value;
    SPI_WAIT(timeout);
    DAP_SPI_End(value);
}

/* Keep pin ownership for the entire WDATA phase. Return the byte XOR;
 * SW_DP applies its existing hardware parity helper and GPIO parity bit. */
UINT8 DAP_SPI_WriteWord(const UINT8 idata *datas)
{
    UINT8 n, value, parity, timeout;
#if DAP_SPI_SWD_PREFETCH
    UINT8 next;
#endif
    parity = 0U;
    value = 0U;
    if (DAP_SPI_Failed) { return parity; }
#if DAP_SPI_SWD_PREFETCH
    value = *datas++;
    n = 4U;
    SPI_BEGIN();
    for (;;) {
        SPI0_DATA = value;
        parity ^= value;
        /* 当前字节由外设移位时准备下一字节，不提前覆盖SPI FIFO。
         * 最后一字节不再读取datas；超时仍用最后实际发送值交回GPIO。
         */
        if (--n) { next = *datas++; }
        SPI_WAIT(timeout);
        if (DAP_SPI_Failed || !n) { break; }
        value = next;
    }
#else
    SPI_BEGIN();
    for (n = 0U; n < 4U; n++) {
        value = datas[n];
        SPI0_DATA = value;
        parity ^= value;
        SPI_WAIT(timeout);
        if (DAP_SPI_Failed) { break; }
    }
#endif
    DAP_SPI_End(value);
    return parity;
}
#endif
#endif
