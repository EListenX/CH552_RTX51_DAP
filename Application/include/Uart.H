/********************************** (C) COPYRIGHT *******************************
* File Name          : Uart.H
* Author             : EListen
* Version            : V1.00
* Date               : 2022-07-31
* Description        : 
*******************************************************************************/
#ifndef UART_H
#define UART_H

#include "Usb.H"

// 0: 任意波特率最优计算；1: 常用波特率查表，未列出的请求拒绝并保留原设置。
// 两个方案在编译期选择，不同时占用 ROM。默认保持任意波特率支持。
#ifndef UART_BAUD_USE_TABLE
#define UART_BAUD_USE_TABLE 0
#endif
#if UART_BAUD_USE_TABLE != 0 && UART_BAUD_USE_TABLE != 1
#error UART_BAUD_USE_TABLE must be 0 or 1
#endif



#define CDC_EP_BUF_SIZE    THIS_ENDP0_SIZE
#define CDC_PINGPONG_NUM   2

// 独立取证引脚：P3.2 每次提交翻转，P3.3 每次 IN 完成翻转。
// 只标记软件事件，不表示物理 USB token/ACK；默认不占引脚。
#ifndef CDC_IN_GPIO_DIAG
#define CDC_IN_GPIO_DIAG 0
#endif
#if CDC_IN_GPIO_DIAG != 0 && CDC_IN_GPIO_DIAG != 1
#error CDC_IN_GPIO_DIAG must be 0 or 1
#endif
#if CDC_IN_GPIO_DIAG
#include "Board_Config.H"
#if !defined(DAP_JTAG) || DAP_JTAG || (defined(DAP_HAS_NTRST) && DAP_HAS_NTRST) || BOARD_HAS_LED || BOARD_DIR_CONTROL_SUPPORT
#error CDC GPIO diagnostics require JTAG, nTRST and board LEDs disabled
#endif
#if (PIN_nRESET_PORT == 3 && (PIN_nRESET_BIT == 2 || PIN_nRESET_BIT == 3)) || (PIN_SWCLK_PORT == 3 && (PIN_SWCLK_BIT == 2 || PIN_SWCLK_BIT == 3)) || (PIN_SWDIO_PORT == 3 && (PIN_SWDIO_BIT == 2 || PIN_SWDIO_BIT == 3))
#error CDC GPIO diagnostics conflict with debug pins
#endif
sbit CDC_IN_ARM_PIN = P3 ^ 2;
sbit CDC_IN_DONE_PIN = P3 ^ 3;
#define CDC_IN_ARM_MARK() (CDC_IN_ARM_PIN = !CDC_IN_ARM_PIN)
#define CDC_IN_DONE_MARK() (CDC_IN_DONE_PIN = !CDC_IN_DONE_PIN)
#define CDC_IN_MARK_RESET() do { CDC_IN_ARM_PIN = 0; CDC_IN_DONE_PIN = 0; } while (0)
#else
#define CDC_IN_ARM_MARK()
#define CDC_IN_DONE_MARK()
#define CDC_IN_MARK_RESET()
#endif

// Priority repair candidate; 0 retains the old image for a controlled comparison.
// PS/PT1 are sbit lvalues, not masks. Promote after hardware regression.
#ifndef CDC_UART_PRIORITY_FIX
#define CDC_UART_PRIORITY_FIX 0
#endif
#if CDC_UART_PRIORITY_FIX != 0 && CDC_UART_PRIORITY_FIX != 1
#error CDC_UART_PRIORITY_FIX must be 0 or 1
#endif

// 独立聚包候选：64 字节立即提交，短包由专用 Timer1 周期提交。
// Timer0 仍由 RTX 使用，Timer2 仍用于 UART0 任意波特率。
#ifndef CDC_RX_BATCH
#define CDC_RX_BATCH 0
#endif
#if CDC_RX_BATCH != 0 && CDC_RX_BATCH != 1
#error CDC_RX_BATCH must be 0 or 1
#endif

// Isolated short-packet timing comparison; full packets still wake Timer1.
#ifndef CDC_RX_BATCH_MS
#define CDC_RX_BATCH_MS 1
#endif
#if CDC_RX_BATCH_MS == 1
#define CDC_RX_BATCH_RELOAD 0xF830U
#elif CDC_RX_BATCH_MS == 2
#define CDC_RX_BATCH_RELOAD 0xF060U
#else
#error CDC_RX_BATCH_MS must be 1 or 2
#endif
#if CDC_RX_BATCH_MS != 1 && !CDC_RX_BATCH
#error CDC_RX_BATCH_MS requires CDC_RX_BATCH
#endif

// Optional spill queue; USB DMA slots and packet length stay unchanged.
#ifndef CDC_RX_FIFO
#define CDC_RX_FIFO 0
#endif
#if CDC_RX_FIFO != 0 && CDC_RX_FIFO != 1
#error CDC_RX_FIFO must be 0 or 1
#endif
#if CDC_RX_FIFO && !CDC_RX_BATCH
#error CDC_RX_FIFO requires CDC_RX_BATCH
#endif

// UART-only producer, USB/Timer1 consumer; legacy spill mode stays available.
#ifndef CDC_RX_RING
#define CDC_RX_RING 0
#endif
#if CDC_RX_RING != 0 && CDC_RX_RING != 1
#error CDC_RX_RING must be 0 or 1
#endif
#if CDC_RX_RING && !CDC_RX_FIFO
#error CDC_RX_RING requires CDC_RX_FIFO
#endif

#ifndef CDC_RX_RING_SIZE
#define CDC_RX_RING_SIZE 256
#endif
#if CDC_RX_RING_SIZE != 128 && CDC_RX_RING_SIZE != 256
#error CDC_RX_RING_SIZE must be 128 or 256
#endif
#if CDC_RX_RING_SIZE != 256 && !CDC_RX_RING
#error CDC_RX_RING_SIZE requires CDC_RX_RING
#endif

#ifndef CDC_RX_IDLE_ZLP
#define CDC_RX_IDLE_ZLP 0
#endif
#if CDC_RX_IDLE_ZLP != 0 && CDC_RX_IDLE_ZLP != 1
#error CDC_RX_IDLE_ZLP must be 0 or 1
#endif
#if CDC_RX_IDLE_ZLP && !CDC_RX_RING
#error CDC_RX_IDLE_ZLP requires CDC_RX_RING
#endif

// Optional DAP pacing under RX backlog; never blocks UART or USB interrupts.
#ifndef CDC_DAP_YIELD
#define CDC_DAP_YIELD 0
#endif
#if CDC_DAP_YIELD != 0 && CDC_DAP_YIELD != 1
#error CDC_DAP_YIELD must be 0 or 1
#endif
#if CDC_DAP_YIELD && !CDC_RX_RING
#error CDC_DAP_YIELD requires CDC_RX_RING
#endif

// 独立对照：EP2 的 TOG 只由 USB ISR 在成功事务后推进，默认保留硬件自动翻转。
#ifndef CDC_EP2_MANUAL_TOG
#define CDC_EP2_MANUAL_TOG 0
#endif
#if CDC_EP2_MANUAL_TOG != 0 && CDC_EP2_MANUAL_TOG != 1
#error CDC_EP2_MANUAL_TOG must be 0 or 1
#endif
#if CDC_EP2_MANUAL_TOG
#define CDC_EP2_INIT_CTRL (UEP_T_RES_NAK | UEP_R_RES_ACK)
#else
#define CDC_EP2_INIT_CTRL (bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK)
#endif

// 临时诊断：只在实际 RX 溢出时计数，不在每字节正常路径增加计数操作。
#ifndef CDC_DIAGNOSTICS
#define CDC_DIAGNOSTICS 1
#endif
#define CDC_DIAG_COMMAND 0x80U
#ifndef CDC_RX_SNAPSHOT_DIAG
#define CDC_RX_SNAPSHOT_DIAG 0
#endif
#if CDC_RX_SNAPSHOT_DIAG != 0 && CDC_RX_SNAPSHOT_DIAG != 1
#error CDC_RX_SNAPSHOT_DIAG must be 0 or 1
#endif
#if CDC_RX_SNAPSHOT_DIAG && !CDC_DIAGNOSTICS
#error CDC_RX_SNAPSHOT_DIAG requires CDC_DIAGNOSTICS
#endif
#ifndef CDC_RX_SNAPSHOT_COMPACT
#define CDC_RX_SNAPSHOT_COMPACT 0
#endif
#if CDC_RX_SNAPSHOT_COMPACT != 0 && CDC_RX_SNAPSHOT_COMPACT != 1
#error CDC_RX_SNAPSHOT_COMPACT must be 0 or 1
#endif
#if CDC_RX_SNAPSHOT_COMPACT && (!CDC_RX_SNAPSHOT_DIAG || !CDC_RX_RING)
#error Compact snapshot requires RX snapshot diagnostics and RX ring
#endif
#ifndef CDC_RX_SNAPSHOT_IRQ
#define CDC_RX_SNAPSHOT_IRQ 0
#endif
#if CDC_RX_SNAPSHOT_IRQ != 0 && CDC_RX_SNAPSHOT_IRQ != 1
#error CDC_RX_SNAPSHOT_IRQ must be 0 or 1
#endif
#if CDC_RX_SNAPSHOT_IRQ && !CDC_RX_SNAPSHOT_COMPACT
#error CDC_RX_SNAPSHOT_IRQ requires compact RX snapshot
#endif
#ifndef CDC_RX_SNAPSHOT_SERVICE
#define CDC_RX_SNAPSHOT_SERVICE 0
#endif
#if CDC_RX_SNAPSHOT_SERVICE != 0 && CDC_RX_SNAPSHOT_SERVICE != 1
#error CDC_RX_SNAPSHOT_SERVICE must be 0 or 1
#endif
#if CDC_RX_SNAPSHOT_SERVICE && (!CDC_RX_SNAPSHOT_COMPACT || CDC_RX_SNAPSHOT_IRQ)
#error Service snapshot requires compact snapshot and excludes IRQ-only snapshot
#endif
#ifndef CDC_IN_SNAPSHOT_DIAG
#define CDC_IN_SNAPSHOT_DIAG 0
#endif
#if CDC_IN_SNAPSHOT_DIAG != 0 && CDC_IN_SNAPSHOT_DIAG != 1
#error CDC_IN_SNAPSHOT_DIAG must be 0 or 1
#endif
#if CDC_RX_SNAPSHOT_DIAG && CDC_IN_SNAPSHOT_DIAG
#error Select only one CDC snapshot to preserve the 8 byte RAM budget
#endif
#if CDC_RX_SNAPSHOT_DIAG
#if CDC_RX_SNAPSHOT_COMPACT
#if CDC_RX_SNAPSHOT_SERVICE
#define CDC_RX_SNAPSHOT_SIZE 4U
#define CDC_DIAG_RESPONSE_SIZE 18U
#define CDC_DIAG_VERSION 7U
#else
#define CDC_RX_SNAPSHOT_SIZE 3U
#define CDC_DIAG_RESPONSE_SIZE 17U
#if CDC_RX_SNAPSHOT_IRQ
#define CDC_DIAG_VERSION 6U
#else
#define CDC_DIAG_VERSION 5U
#endif
#endif
#else
#define CDC_RX_SNAPSHOT_SIZE 8U
#define CDC_DIAG_RESPONSE_SIZE 22U
#define CDC_DIAG_VERSION 4U
#endif
#elif CDC_IN_SNAPSHOT_DIAG
#define CDC_DIAG_RESPONSE_SIZE 22U
#define CDC_DIAG_VERSION 3U
#else
#define CDC_DIAG_RESPONSE_SIZE 14U
#define CDC_DIAG_VERSION 2U
#endif

// 独立诊断构建：IN 完成时核对自动 TOG 与软件聚包通道，不改变交接行为。
#ifndef CDC_IN_HANDOFF_DIAG
#define CDC_IN_HANDOFF_DIAG 0
#endif
#if CDC_IN_HANDOFF_DIAG != 0 && CDC_IN_HANDOFF_DIAG != 1
#error CDC_IN_HANDOFF_DIAG must be 0 or 1
#endif
#if CDC_IN_HANDOFF_DIAG && !CDC_DIAGNOSTICS
#error CDC_IN_HANDOFF_DIAG requires CDC_DIAGNOSTICS
#endif
#if CDC_IN_SNAPSHOT_DIAG && !CDC_IN_HANDOFF_DIAG
#error CDC_IN_SNAPSHOT_DIAG requires CDC_IN_HANDOFF_DIAG
#endif

typedef struct {
    volatile UINT8 flags;         // CDC state flags
    volatile UINT8 tx_length[CDC_PINGPONG_NUM];
    volatile UINT8 tx_index;
    volatile UINT8 rx_length;
#if CDC_DIAGNOSTICS
    volatile UINT8 rx_dropped[2]; // 小端累计丢弃数，USB reset 清零，65536 自然回绕。
    volatile UINT8 usb_out_bytes[2];
    volatile UINT8 uart_tx_bytes[2];
    volatile UINT8 uart_rx_bytes[2];
    volatile UINT8 usb_in_bytes[2];
#if CDC_IN_SNAPSHOT_DIAG
    // 首次失配：CTRL、T_LEN、flags、rx_length、RX/IN 小端累计数。
    // 由 flags.bit7 判断有效；USB reset 后失效，下一次失配覆盖全部字段。
    volatile UINT8 in_snapshot[8];
#elif CDC_RX_SNAPSHOT_DIAG
    // 首次溢出：CTRL、T_LEN、flags、rx_length、RX 小端计数、USB FG/ST。
    // bit6 是有效标志；USB reset 清标志，旧内容不返回主机。
    // Compact v5: CTRL, flags, rx_length only; no fabricated omitted fields.
      // IRQ v6: USB_INT_FG, USB_INT_ST, TCON; consecutive register samples.
    volatile UINT8 rx_snapshot[CDC_RX_SNAPSHOT_SIZE];
#endif
#endif
} tCDCState;

#if CDC_RX_FIFO
#if CDC_RX_RING
// Reserve one slot to distinguish full from empty with two 8-bit indices.
extern volatile UINT8 xdata CDC_RxFifo[CDC_RX_RING_SIZE];
extern volatile UINT8 data CDC_RxWrite, CDC_RxRead;
#else
extern volatile UINT8 xdata CDC_RxFifo[128];
extern volatile UINT8 data CDC_RxWrite, CDC_RxRead, CDC_RxCount;
#endif
#if CDC_RX_RING
// One byte, called with UART masked. No shared automatic RAM in this leaf.
UINT8 CDC_RxDrain(void);
// Each ISR owns its ES save; only the register-only copy leaf is shared.
#define CDC_RxPump() do { \
    bit cdc_rx_saved_es; \
    UINT8 cdc_rx_copied; \
    do { \
        cdc_rx_saved_es = ES; \
        ES = 0; \
        cdc_rx_copied = CDC_RxDrain(); \
        ES = cdc_rx_saved_es; \
        _nop_(); \
    } while (cdc_rx_copied); \
} while (0)
#else
void CDC_RxDrain(void);
#endif
#endif

#if CDC_DIAGNOSTICS
// 不使用 16 位算术；USB 更新计数时已屏蔽 UART，查询时屏蔽中断。
#define CDC_DIAG_INC(field) do { \
    if (++CDC.field[0] == 0U) { ++CDC.field[1]; } \
} while (0)
#define CDC_DIAG_ADD(field, amount) do { \
    UINT8 cdc_diag_amount = (amount); \
    CDC.field[0] += cdc_diag_amount; \
    if (CDC.field[0] < cdc_diag_amount) { ++CDC.field[1]; } \
} while (0)
#endif


// CDC channel / state bits
#define CDC_TX_CHANNEL_BIT      0   // Bit0 = current TX channel (0/1)
#define CDC_RX_CHANNEL_BIT      1   // Bit1 = current RX channel (0/1)

#define CDC_TX_STATE0_BIT      2   // Bit2 = TX0 state (1=busy)
#define CDC_TX_STATE1_BIT      3   // Bit3 = TX1 state (1=busy)
#define CDC_RX_STATE0_BIT      4   // Bit4 = RX0 state (1=data ready)
#define CDC_RX_STATE1_BIT      5   // Bit5 = RX1 state (1=data ready)
#define CDC_RX_OVERFLOW_BIT      6   // Bit6 = RX overflow (sticky until USB reset)
#define CDC_RX_CHANNEL_MISMATCH_BIT 7 // 仅诊断构建使用，USB reset 清零。


// Channel ops
#define CDC_GetTxChannel()     ((CDC.flags >> CDC_TX_CHANNEL_BIT) & 0x01)
#define CDC_SetTxChannel(ch)   (CDC.flags = (CDC.flags & ~(1<<CDC_TX_CHANNEL_BIT)) | ((ch&0x01)<<CDC_TX_CHANNEL_BIT))
#define CDC_ToggleTxChannel()  (CDC.flags ^= (1<<CDC_TX_CHANNEL_BIT))

#define CDC_GetRxChannel()     ((CDC.flags >> CDC_RX_CHANNEL_BIT) & 0x01)
#define CDC_SetRxChannel(ch)   (CDC.flags = (CDC.flags & ~(1<<CDC_RX_CHANNEL_BIT)) | ((ch&0x01)<<CDC_RX_CHANNEL_BIT))
#define CDC_ToggleRxChannel()  (CDC.flags ^= (1<<CDC_RX_CHANNEL_BIT))

// Isolated equivalent mask implementation; all callers use channel 0 or 1.
#ifndef CDC_STATE_MASK_FAST
#define CDC_STATE_MASK_FAST 0
#endif
#if CDC_STATE_MASK_FAST != 0 && CDC_STATE_MASK_FAST != 1
#error CDC_STATE_MASK_FAST must be 0 or 1
#endif
#if CDC_STATE_MASK_FAST
// Fixed masks avoid variable-width shifts on the 8051. Arguments evaluated once.
#define CDC_SetTxState(ch, val) do { \
    if ((val) & 1U) { CDC.flags |= (ch) ? 0x08U : 0x04U; } \
    else { CDC.flags &= ~((ch) ? 0x08U : 0x04U); } \
} while (0)
#define CDC_GetTxState(ch) ((CDC.flags & ((ch) ? 0x08U : 0x04U)) != 0U)
#define CDC_SetRxState(ch, val) do { \
    if ((val) & 1U) { CDC.flags |= (ch) ? 0x20U : 0x10U; } \
    else { CDC.flags &= ~((ch) ? 0x20U : 0x10U); } \
} while (0)
#define CDC_GetRxState(ch) ((CDC.flags & ((ch) ? 0x20U : 0x10U)) != 0U)
#else
// Historical variable-shift control.
#define CDC_SetTxState(ch, val)  \
    (CDC.flags = (CDC.flags & ~(1U << (CDC_TX_STATE0_BIT + (ch)))) | (((val)&1U) << (CDC_TX_STATE0_BIT + (ch))))
#define CDC_GetTxState(ch)       \
    (((CDC.flags >> (CDC_TX_STATE0_BIT + (ch))) & 0x01U))

#define CDC_SetRxState(ch, val)  \
    (CDC.flags = (CDC.flags & ~(1U << (CDC_RX_STATE0_BIT + (ch)))) | (((val)&1U) << (CDC_RX_STATE0_BIT + (ch))))
#define CDC_GetRxState(ch)       \
    (((CDC.flags >> (CDC_RX_STATE0_BIT + (ch))) & 0x01U))
#endif



// Global CDC state
extern idata tCDCState CDC;

//typedef union
//{
//	volatile UINT8 byte;
//	struct 
//	{
//		volatile UINT8 bit0: 1;
//		volatile UINT8 bit1: 1;
//		volatile UINT8 bit2: 1;
//		volatile UINT8 bit3: 1;
//		volatile UINT8 bit4: 1;
//		volatile UINT8 bit5: 1;
//		volatile UINT8 bit6: 1;
//		volatile UINT8 bit7: 1;
//	}bits;  
//}tFlag;

//typedef union {
//  PUINT32I time32;
//  PUINT16I time16[2];
//  PUINT8I time8[4];
//} osalTime_t;

UINT8 Config_Uart0(UINT8 xdata *cfg_uart);
void CDC_printf(char *fmt,...);
void CDC_Send(char * targetString);
void InitUart0(void);


#endif
/* END OF FILE */
