
/********************************** (C) COPYRIGHT *******************************
 * File Name          : Uart.C
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2017/11/18
 * Description        : UART and CDC virtual COM implementation
 * Features           : 1. UART init/config
 *                      2. CDC VCOM RX/TX
 *                      3. Formatted print output
 *******************************************************************************/
#include "CH552.H"
#include "Debug.H"
#include "Uart.H"
#include "stdio.h"
#include "string.h"
#include "stdarg.h"

//#include <rtx51tny.h> /* RTX-51 tiny functions & defines      */
#include "Config.H"
#include "CH55x_Math.H"
#if CDC_RX_FIFO
#include <intrins.h>
#endif
#if CDC_RX_FIFO
#if CDC_RX_RING
volatile UINT8 xdata CDC_RxFifo[CDC_RX_RING_SIZE];
volatile UINT8 data CDC_RxWrite, CDC_RxRead;
#else
volatile UINT8 xdata CDC_RxFifo[128];
volatile UINT8 data CDC_RxWrite, CDC_RxRead, CDC_RxCount;
#endif
#endif
/* BEGIN UART_Baud */
// UART0 波特率发生器映射，固定 24 MHz；Timer2 不占用 RTX 的 Timer0。
#if FREQ_SYS != 24000000UL
#error UART0 baud mapping requires 24 MHz FREQ_SYS
#endif
#define UART_CONFIG_REJECTED 0U
#define UART_CONFIG_ACCEPTED 1U
#define UART_BAUD_MIN 6UL
#define UART_BAUD_MAX 1500000UL

#if UART_BAUD_USE_TABLE
// 与计算方案使用相同的最近实际频率；请求值不一定能精确产生。
// 所有表项均使用 Timer2 快档，重装值的高字节均为 FF。
static const UINT32 code UART_BaudRates[] = {
    9600UL, 19200UL, 38400UL, 57600UL, 115200UL,
    230400UL, 460800UL, 921600UL, 1000000UL, 1500000UL
};
static const UINT8 code UART_BaudReloadLow[] = {
    0x64U, 0xB2U, 0xD9U, 0xE6U, 0xF3U,
    0xF9U, 0xFDU, 0xFEU, 0xFEU, 0xFFU
};
#endif

// 仅配置算法优先代码体积，恢复后 UART 收发中断仍按工程 SPEED 编译。
#pragma SAVE
#pragma OPTIMIZE(8, SIZE)
static UINT8 UART0ApplyBaud(UINT32 baud)
{
#if UART_BAUD_USE_TABLE
    UINT8 index;
#else
    UINT16 count;
#endif
    UINT16 reload;
    bit saved_es;
    bit slow;

#if UART_BAUD_USE_TABLE
    for (index = 0; index < sizeof(UART_BaudReloadLow); index++) {
        if (baud == UART_BaudRates[index]) {
            break;
        }
    }
    if (index == sizeof(UART_BaudReloadLow)) {
        return UART_CONFIG_REJECTED;
    }
    slow = 0;
    reload = 0xFF00U | UART_BaudReloadLow[index];
#else
    if (baud - UART_BAUD_MIN > UART_BAUD_MAX - UART_BAUD_MIN) {
        return UART_CONFIG_REJECTED;
    }
    // 快档已覆盖慢档的有效候选；低于快档最低值时使用慢档。
    slow = baud < 23UL;
    // 专用内核同时得到商/余数并比较实际频率误差，不调用通用长整数乘除法。
    count = (UINT16)CH55x_ClockDivide(baud |
        (slow ? CH55X_CLOCK_UART_SLOW : CH55X_CLOCK_UART_FAST));
    reload = (UINT16)(0U - count);
#endif
    saved_es = ES;
    ES = 0;
    T2CON = 0x30U; // 停止 Timer2；RX/TX 内部时钟、自动重装，不影响 Timer1。
    T2MOD = (T2MOD & ~bT2_CLK) | bTMR_CLK;
    if (!slow) {
        T2MOD |= bT2_CLK;
    }
    RCAP2L = (UINT8)reload;
    RCAP2H = (UINT8)(reload >> 8);
    TL2 = RCAP2L;
    TH2 = RCAP2H;
    TR2 = 1;
    ES = saved_es;
    return UART_CONFIG_ACCEPTED;
}
#pragma RESTORE
/* END UART_Baud */

idata tCDCState CDC = {0};

/*******************************************************************************
 * Function Name  : CDC_printf
 * Description    : Formatted print over CDC virtual COM
 * Input          : fmt  - format string (printf-style)
 *                  ...  - variadic arguments
 * Output         : None
 * Return         : None
 * Notes          : 1. Output length must not exceed endpoint buffer
 *                 2. Double-buffer improves throughput
 *******************************************************************************/
void CDC_printf(char *fmt,...)
{
    unsigned char len;
    char* ap;                    // Variadic argument pointer
    unsigned char rx_ch;

    rx_ch = CDC_GetRxChannel();  // Get current RX channel

//    EA = 0;                   // Disable interrupts (unused)

    va_start(ap,fmt);           // Start variadic args
    len=vsprintf((UIS_EP2_IN_BUF(rx_ch)),fmt,ap);  // Format into buffer
    va_end(ap);                 // End variadic args

    // Clamp to endpoint buffer size
    if (len > CDC_EP_BUF_SIZE)
        len = CDC_EP_BUF_SIZE;

    UEP2_T_LEN = 0;            // Clear length register
    UEP2_T_LEN = len;          // Set TX length
    // Set endpoint status to ACK to allow IN transfer
    UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;

    CDC_ToggleRxChannel();      // Toggle RX channel

//    EA = 1;
}

/*******************************************************************************
 * Function Name  : CDC_Send
 * Description    : Send a string over CDC virtual COM
 * Input          : targetString - string to send
 * Output         : None
 * Return         : None
 * Notes          : 1. Length must not exceed endpoint buffer
 *                 2. Double-buffering improves throughput
 *******************************************************************************/
void CDC_Send(char * targetString)
{
    unsigned char rx_ch;
    unsigned char len;

    rx_ch = CDC_GetRxChannel();  // Get current RX channel

    len = strlen(targetString);  // String length

    // Clamp to endpoint buffer size
    if (len > CDC_EP_BUF_SIZE)
        len = CDC_EP_BUF_SIZE;

    // Copy to USB IN buffer
    strncpy((char *)(UIS_EP2_IN_BUF(rx_ch)), targetString, len);

    UEP2_T_LEN = len;           // Set TX length
    // Set endpoint status to ACK to allow IN transfer
    UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;

    CDC_ToggleRxChannel();       // Toggle RX channel
}

/*******************************************************************************
 * Function Name  : InitUart0
 * Description    : Init CH552 UART0 (Timer2 baudrate generator)
 * Input          : None (57600 default)
 * Output         : None
 * Return         : None
 * Notes          : 8N1, variable baudrate
 *******************************************************************************/

void InitUart0(void)
{
#if CDC_IN_GPIO_DIAG
    CDC_IN_MARK_RESET();
    P3_MOD_OC &= ~0x0CU;
    P3_DIR_PU |= 0x0CU;
#endif
    // 默认 57600 的映射结果：1500000/26，重装值 FFE6。
    // 初始化不调用 USB ISR 所用算法函数，避免 C51 非重入局部存储冲突。
    T2MOD |= bTMR_CLK | bT2_CLK;
    RCAP2L = 0xE6U;
    RCAP2H = 0xFFU;
    TL2 = RCAP2L;
    TH2 = RCAP2H;
    T2CON = 0x34U;

#if CDC_RX_BATCH
    // Timer1 专用聚包周期：24 MHz / 12，默认 1 ms，独立候选 2 ms。
    // 不复用 RTX Timer0，不改 Timer2 的 UART 波特率发生器。
    TR1 = 0;
    TMOD = (TMOD & 0x0FU) | 0x10U;
    T2MOD &= ~bT1_CLK;
    TH1 = (UINT8)(CDC_RX_BATCH_RELOAD >> 8);
    TL1 = (UINT8)CDC_RX_BATCH_RELOAD;
    TF1 = 0;
#if CDC_UART_PRIORITY_FIX
    PT1 = 0; // sbit assignment preserves every other IP bit.
#else
    IP &= ~PT1; // Legacy comparison only: PT1 is not a mask.
#endif
    ET1 = 1;
    TR1 = 1;
#endif

    // UART mode
    SCON = 0x50;             // Mode 1, REN=1，RI/TI 清零。

    // Interrupt
#if CDC_UART_PRIORITY_FIX
    PS = 1;                  // High priority: SETB IP.4, not OR with its value.
#else
    IP |= PS;                // Legacy comparison only: does not set PS from 0.
#endif
    ES = 1;                  // Enable UART interrupt
}
/*******************************************************************************
 * Function Name  : Config_Uart0
 * Description    : Configure UART0 baudrate from CDC line coding
 * Input          : line_code - CDC line coding (baudrate etc.)
 * Output         : None
 * Return         : 1 accepted, 0 rejected (previous configuration retained)
 *******************************************************************************/
UINT8 Config_Uart0(UINT8 xdata *line_code)
{
    UINT32 buadrate; // 四个字节均会赋值，无需额外清零。

    // Extract baudrate from CDC (little-endian)
    ((UINT8 xdata *) &buadrate)[0] = line_code[3];
    ((UINT8 xdata *) &buadrate)[1] = line_code[2];
    ((UINT8 xdata *) &buadrate)[2] = line_code[1];
    ((UINT8 xdata *) &buadrate)[3] = line_code[0];

    return UART0ApplyBaud(buadrate); // 拒绝范围外值，不改动 UART 寄存器。
}

/*******************************************************************************
 * Function Name  : UART0Interrupt
 * Description    : UART0 ISR (CDC RX/TX bridge)
 * Notes          : Uses register bank 3 to avoid conflicts
 *******************************************************************************/
#if CDC_RX_RING && CDC_RX_RING_SIZE == 128
// Keep the smaller-ring candidate below ISP without changing the speed policy.
#pragma SAVE
#pragma OPTIMIZE(9,SPEED)
#endif
void UART0Interrupt(void) interrupt INT_NO_UART0 using 3
{
    UINT8 cdc_ch;           // CDC channel (double-buffer)

    // RX interrupt
    if(RI)
    {
#if CDC_DIAGNOSTICS
        CDC_DIAG_INC(uart_rx_bytes);
#endif
#if CDC_RX_RING
        // UART never writes DMA or RX flags in ring mode.
#if CDC_RX_RING_SIZE == 128
        cdc_ch = (CDC_RxWrite + 1U) & 0x7FU;
        if (cdc_ch != CDC_RxRead)
#else
        if ((UINT8)(CDC_RxWrite + 1U) != CDC_RxRead)
#endif
        {
#if CDC_RX_RING_SIZE == 128
            CDC_RxFifo[CDC_RxWrite] = SBUF;
            CDC_RxWrite = cdc_ch;
#else
            CDC_RxFifo[CDC_RxWrite++] = SBUF;
#endif
            // Wake the USB-side consumer at one packet; do not wait a timer
            // period for every full packet at high baud. UART still owns no DMA.
#if CDC_RX_RING_SIZE == 128
            // Seven-bit indices differ by 64 modulo 128 iff their XOR is 64.
            if ((CDC_RxWrite ^ CDC_RxRead) == CDC_EP_BUF_SIZE) { TF1 = 1; }
#else
            if ((UINT8)(CDC_RxWrite - CDC_RxRead) == CDC_EP_BUF_SIZE) { TF1 = 1; }
#endif
        }
#else
        cdc_ch = CDC_GetRxChannel();  // Get current RX channel

        // 聚包不跨越 64 字节 DMA 边界；无流控且主机停读时记录溢出。
        if (CDC.rx_length < CDC_EP_BUF_SIZE
#if CDC_RX_FIFO
            && !CDC_RxCount
#endif
        )
        {
            CDC_SetRxState(cdc_ch, 1);
            (UIS_EP2_IN_BUF(cdc_ch))[CDC.rx_length++] = SBUF;
        }
#if CDC_RX_FIFO
        else if (CDC_RxCount < 128U)
        {
            CDC_RxFifo[CDC_RxWrite] = SBUF;
            CDC_RxWrite = (CDC_RxWrite + 1U) & 0x7FU;
            CDC_RxCount++;
        }
#endif
#endif
        else
        {
            // 本字节不写入 DMA，随后清 RI，不能覆盖相邻 XRAM。
#if CDC_RX_SNAPSHOT_DIAG
            if (!(CDC.flags & (1U << CDC_RX_OVERFLOW_BIT))) {
                // UART 高优先级；只锁存首次溢出，正常接收路径不增加快照操作。
#if CDC_RX_SNAPSHOT_SERVICE
                CDC.rx_snapshot[0] = USB_INT_FG;
                CDC.rx_snapshot[1] = UEP2_CTRL;
                CDC.rx_snapshot[2] = CDC.flags;
                CDC.rx_snapshot[3] = CDC.rx_length;
#elif CDC_RX_SNAPSHOT_IRQ
                CDC.rx_snapshot[0] = USB_INT_FG;
                CDC.rx_snapshot[1] = USB_INT_ST;
                CDC.rx_snapshot[2] = TCON;
#else
                CDC.rx_snapshot[0] = UEP2_CTRL;
#if CDC_RX_SNAPSHOT_COMPACT
                CDC.rx_snapshot[1] = CDC.flags;
                CDC.rx_snapshot[2] = CDC.rx_length;
#else
                CDC.rx_snapshot[1] = UEP2_T_LEN;
                CDC.rx_snapshot[2] = CDC.flags;
                CDC.rx_snapshot[3] = CDC.rx_length;
                CDC.rx_snapshot[4] = CDC.uart_rx_bytes[0];
                CDC.rx_snapshot[5] = CDC.uart_rx_bytes[1];
                CDC.rx_snapshot[6] = USB_INT_FG;
                CDC.rx_snapshot[7] = USB_INT_ST;
#endif
#endif
            }
#endif
            CDC.flags |= (1U << CDC_RX_OVERFLOW_BIT);
#if CDC_DIAGNOSTICS
            if (++CDC.rx_dropped[0] == 0U) { ++CDC.rx_dropped[1]; }
#endif
        }

#if !CDC_RX_RING
        // If the other buffer is free, arm USB IN transfer
        if (!CDC_GetRxState(cdc_ch ^ 1)
#if CDC_RX_BATCH
            && CDC.rx_length == CDC_EP_BUF_SIZE
#endif
        )
        {
            UEP2_T_LEN = 0;             // Clear length register
            UEP2_T_LEN = CDC.rx_length; // Set TX length
            // ACK to allow IN transfer
            UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
            CDC_IN_ARM_MARK();
            CDC.rx_length = 0;          // Reset RX length
            CDC_ToggleRxChannel();      // Toggle RX channel
        }
#endif
        RI = 0;                        // Clear RI
    }

    // TX interrupt
    if(TI)
    {
        cdc_ch = CDC_GetTxChannel();   // Get current TX channel

        // Continue sending current channel
        if(CDC_GetTxState(cdc_ch) && CDC.tx_index < CDC.tx_length[cdc_ch])
        {
            SBUF = (UIS_EP2_OUT_BUF(cdc_ch))[CDC.tx_index++];
#if CDC_DIAGNOSTICS
            CDC_DIAG_INC(uart_tx_bytes);
#endif
        }
        else
        {
            CDC.tx_index = 0;
            CDC_SetTxState(cdc_ch, 0);  // Clear busy flag
            // 当前 DMA 缓冲已释放，允许下一包 OUT；即使没有排队包也要解除 NAK。
            UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;

            // If the other channel is pending, switch and start it
            if(CDC_GetTxState(cdc_ch ^ 1))
            {
                CDC_ToggleTxChannel(); // Toggle TX channel
                cdc_ch ^= 1;
                SBUF = (UIS_EP2_OUT_BUF(cdc_ch))[CDC.tx_index++];
#if CDC_DIAGNOSTICS
                CDC_DIAG_INC(uart_tx_bytes);
#endif
            }
        }
        TI = 0;                         // Clear TI
    }
}

#if CDC_RX_RING && CDC_RX_RING_SIZE == 128
#pragma RESTORE
#endif
#if CDC_RX_FIFO
// Ring: masked single-byte leaf shared by non-nesting USB/Timer1 bank 1 ISRs.
// Legacy spill: USB-only drain manages its own per-byte ES critical sections.
#pragma SAVE
#pragma NOAREGS
#pragma OPTIMIZE(8,SIZE)
#if CDC_RX_RING
UINT8 CDC_RxDrain(void)
#else
void CDC_RxDrain(void)
#endif
{
#if CDC_RX_RING
    UINT8 offset;
#if CDC_RX_RING_SIZE == 128
    // UART cannot change staging length; the caller has already masked it.
    offset = CDC.rx_length;
    if (CDC_RxWrite == CDC_RxRead || offset == CDC_EP_BUF_SIZE) {
        return 0U;
    }
    CDC.rx_length = offset + 1U;
#else
    if (CDC_RxWrite == CDC_RxRead || CDC.rx_length == CDC_EP_BUF_SIZE) {
        return 0U;
    }
    offset = CDC.rx_length++;
#endif
#if CDC_RX_RING_SIZE == 128
    if (CDC.flags & (1U << CDC_RX_CHANNEL_BIT))
#else
    if (CDC_GetRxChannel())
#endif
    {
        CDC_SetRxState(1, 1);
        offset += CDC_EP_BUF_SIZE;
    } else {
        CDC_SetRxState(0, 1);
    }
    UIS_EP2_IN_BUF(0)[offset] = CDC_RxFifo[CDC_RxRead++];
#if CDC_RX_RING_SIZE == 128
    CDC_RxRead &= 0x7FU;
#endif
    return 1U;
#else
    bit saved_es;
    UINT8 offset;
    saved_es = ES;
    ES = 0;
    while (CDC_RxCount && CDC.rx_length < CDC_EP_BUF_SIZE) {
        offset = CDC.rx_length++;
        if (CDC_GetRxChannel()) {
            CDC_SetRxState(1, 1);
            offset += CDC_EP_BUF_SIZE;
        } else {
            CDC_SetRxState(0, 1);
        }
        UIS_EP2_IN_BUF(0)[offset] = CDC_RxFifo[CDC_RxRead];
        CDC_RxRead = (CDC_RxRead + 1U) & 0x7FU;
        CDC_RxCount--;
        ES = saved_es;
        // Leave an instruction after the IE write before masking UART again.
        _nop_();
        ES = 0;
    }
    ES = saved_es;
#endif
}
#pragma RESTORE
#endif

#if CDC_RX_BATCH
// 与 USB 同低优先级、同寄存器组，不相互抢占；操作队列时屏蔽 UART 并恢复 ES。
// 周期不是绝对时延保证：更高优先级中断或关中断仍可延后短包提交。
void CDC_RxBatchTimer(void) interrupt INT_NO_TMR1 using 1
{
    UINT8 cdc_ch;
    bit saved_es;

    TH1 = (UINT8)(CDC_RX_BATCH_RELOAD >> 8);
    TL1 = (UINT8)CDC_RX_BATCH_RELOAD;
#if CDC_RX_RING
    CDC_RxPump();
#endif
    saved_es = ES;
    ES = 0;
    cdc_ch = CDC_GetRxChannel();
#if CDC_RX_IDLE_ZLP
    if (!CDC_GetRxState(cdc_ch ^ 1) && (CDC.rx_length
        // End an idle full-packet burst once. T_LEN becomes zero on this arm,
        // preventing repeated ZLPs. An in-flight slot is never reprogrammed.
        || (UEP2_T_LEN == CDC_EP_BUF_SIZE && CDC_RxWrite == CDC_RxRead)
    ))
#else
    if (CDC.rx_length && !CDC_GetRxState(cdc_ch ^ 1))
#endif
    {
        // 另一槽空闲才提交，不能改写已经 ACK 的 DMA 内容或长度。
#if CDC_RX_IDLE_ZLP
        // A ZLP has no bytes but owns its DMA slot/TOG until completion.
        CDC_SetRxState(cdc_ch, 1);
#endif
        UEP2_T_LEN = CDC.rx_length;
        CDC.rx_length = 0;
        CDC_ToggleRxChannel();
        UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
        CDC_IN_ARM_MARK();
    }
    ES = saved_es;
}
#endif

/*******************************************************************************
 * Function Name  : Uart_Task
 * Description    : UART task: init UART then delete itself
 * Notes          : Init UART0 with 57600 baud
 *******************************************************************************/
void Uart_Task (void) _task_ UART
{
    InitUart0();
//    UART1Init();
    os_delete_task(UART);
}
