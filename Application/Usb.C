
/********************************** (C) COPYRIGHT *******************************
 * File Name          : Usb.C
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2017/11/18
 * Description        : USB device driver and DAP/CDC composite device
 *******************************************************************************/
#include "CH552.H"
#include "Debug.H"
#include <stdio.h>
#include <string.h>

#include "Config.H"
#include "Usb.H"
#include "Usb_Config.H"
#include "Uart.H"
#if CDC_RX_RING
#include <intrins.h>
#endif
#include "Dap.H"
/* BEGIN USB_Serial */
// CH552 出厂 UID 共 40 位：3FFA 的低字节和 3FFC/3FFE 两个小端字。
// 按高位到低位编码，保留全部 5 字节；不使用宽整数或 sprintf。
#ifndef USB_UID_BYTE
#define USB_UID_BYTE(address) (*(PUINT8C)(address))
#endif
#define USB_SERIAL_DESCRIPTOR_LENGTH 22U
UINT8X SerNumber[USB_SERIAL_DESCRIPTOR_LENGTH];

static void USBSerialInit(void)
{
    UINT8I i;
    UINT8 value;
    UINT8C hex[] = "0123456789ABCDEF";

    SerNumber[0] = USB_SERIAL_DESCRIPTOR_LENGTH;
    SerNumber[1] = 3U; // USB string descriptor，内容为 UTF-16LE。
    for (i = 0U; i < 5U; i++) {
        if (i == 0U) {
            value = USB_UID_BYTE(0x3FFA);
        } else {
            value = USB_UID_BYTE(0x4000U - i);
        }
        SerNumber[2U + i * 4U] = hex[value >> 4];
        SerNumber[3U + i * 4U] = 0U;
        SerNumber[4U + i * 4U] = hex[value & 0x0FU];
        SerNumber[5U + i * 4U] = 0U;
    }
}
/* END USB_Serial */

//000
UINT8X Ep0Buffer[THIS_ENDP0_SIZE] _at_ 0x0000;  // EP0 OUT&IN buffer, must be even address (do not change)
//040
UINT8X Ep4Buffer[THIS_ENDP0_SIZE] _at_ 0x0040;  // EP4 OUT&IN buffer, must be even address (do not change)
//080,0C0,100,140
UINT8X Ep1Buffer[4 * THIS_ENDP0_SIZE] _at_ 0x0080; // EP1 OUT&IN double buffer, must be even address (do not change)
//180,1C0,180,200
UINT8X Ep2Buffer[4 * THIS_ENDP0_SIZE] _at_ 0x0180; // EP2 OUT&IN double buffer, must be even address (do not change)
//240

PUINT8 pDescr; // USB descriptor pointer
UINT8I SetupReq, SetupLen, Ready, Count, UsbConfig;

UINT16I USB_STATUS = 0;

/*******************************************************************************
 * Function Name  : USBDeviceInit()
 * Description    : USB device mode init (endpoints + interrupts)
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
void USBDeviceInit()
{
    IE_USB = 0;
    USBSerialInit();        // 在 USB 上拉启用前生成唯一序列号，枚举后保持稳定。
    USB_CTRL = 0x00;        // Set USB device mode
    UDEV_CTRL = bUD_PD_DIS; // Disable DP/DM pull-down
#ifndef Fullspeed
    UDEV_CTRL |= bUD_LOW_SPEED; // Low-speed 1.5M
    USB_CTRL |= bUC_LOW_SPEED;
#else
    UDEV_CTRL &= ~bUD_LOW_SPEED; // Full-speed 12M (default)
    USB_CTRL &= ~bUC_LOW_SPEED;
#endif

    UEP0_DMA = Ep0Buffer;                                      // EP0 DMA address
    UEP4_1_MOD |= bUEP4_RX_EN;                                 // EP0/EP4 single 64-byte buffer
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;                 // OUT=ACK, IN=NAK
    UEP4_CTRL = UEP_T_RES_NAK;                                 // EP4: IN=NAK

    UEP1_DMA = Ep1Buffer;                                      // EP1 DMA address
    UEP4_1_MOD |= bUEP1_BUF_MOD | bUEP1_TX_EN | bUEP1_RX_EN;   // EP1 double 64-byte buffer
    UEP1_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
#if !DAP_EP1_MANUAL_TOG
    UEP1_CTRL |= bUEP_AUTO_TOG; // EP1: IN=NAK, OUT=ACK
#endif

    UEP2_DMA = Ep2Buffer;                                      // EP2 DMA address
    UEP2_3_MOD |= bUEP2_BUF_MOD | bUEP2_TX_EN | bUEP2_RX_EN;   // EP2 double 64-byte buffer
    UEP2_CTRL = CDC_EP2_INIT_CTRL; // EP2: IN=NAK, OUT=ACK

    USB_DEV_AD = 0x00;
    UDEV_CTRL = bUD_PD_DIS;                                    // Disable DP/DM pull-down
    USB_CTRL |= bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;     // Enable device+DMA; auto-NAK until IRQ flags cleared
    UDEV_CTRL |= bUD_PORT_EN;                                  // Enable USB port
    USB_INT_FG = 0xFF;                                         // Clear interrupt flags
    USB_INT_EN = bUIE_SUSPEND | bUIE_TRANSFER | bUIE_BUS_RST;
    IE_USB = 1;
}
/*******************************************************************************
 * Function Name  : DeviceInterrupt()
 * Description    : CH552 USB interrupt handler
 *******************************************************************************/
void DeviceInterrupt(void) interrupt INT_NO_USB using 1 // USB ISR, register bank 1
{
    UINT8 len;
    UINT8 channel;
    bit saved_es; // CDC 共享状态更新期间屏蔽 UART，之后恢复原中断状态。
    if (UIF_TRANSFER) // USB transfer complete
    {
        switch (USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
        {
        case UIS_TOKEN_OUT | 1: // EP1 bulk OUT: DAP command
            if (U_TOG_OK)       // Ignore if data toggle not synchronized
            {
#if DAP_EP1_MANUAL_TOG
                // 同步 OUT 成功后才推进 DMA 槽；重试包不得推进。
                UEP1_CTRL ^= bUEP_R_TOG;
#endif
                channel = !((UEP1_CTRL & bUEP_R_TOG) >> 7);
                if ((USB_RX_LEN != 0U) &&
                    (Ep1Buffer[channel * THIS_ENDP0_SIZE] == ID_DAP_TransferAbort)) {
                    // TransferAbort 没有响应；ISR 直接通知正在运行的重试循环。
                    // 不改变 OUT 通道/队列状态，避免覆盖当前正在处理的命令。
                    DAP_TransferAbort_Flag = 1U;
                } else {
                    DAP_RequestLength[channel] = USB_RX_LEN;
#if DAP_PACKET_COUNT == 2
                    // 保持最早未处理请求，不能让第二包抢先执行。
                    if (!DAP_GET_OUT_STATE(0) && !DAP_GET_OUT_STATE(1)) {
                        DAP_SET_OUT_CH(channel);
                    }
                    DAP_SET_OUT_STATE(channel, 1);
#else
                    DAP_SET_OUT_CH(channel);
                    DAP_SET_OUT_STATE(DAP_GET_OUT_CH(), 1);
#endif
                    isr_send_signal(DAP);
                }
#if DAP_PACKET_COUNT == 2
                // 自动翻转后的下一 DMA 通道仍占用时停止接收，含 Abort 换槽。
                UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_R_RES) |
                    (DAP_GET_OUT_STATE((UEP1_CTRL >> 7) & 1U) ? UEP_R_RES_NAK : UEP_R_RES_ACK);
#else
                UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
#endif
            }
            break;

        case UIS_TOKEN_IN | 1: // EP1 bulk IN: DAP response
#if DAP_EP1_MANUAL_TOG
            // 完成事件由 ISR 推进，同步位不再由硬件异步改写。
            UEP1_CTRL ^= bUEP_T_TOG;
#endif
            channel = ((UEP1_CTRL & bUEP_T_TOG) >> 6);
            DAP_SET_IN_CH(channel);
            DAP_SET_IN_STATE(channel ^ 1, 0);
            isr_send_signal(DAP);
//            UEP1_T_LEN = 0;                                          // Preload length must be cleared
            UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK; // Default NAK
            break;

        case UIS_TOKEN_OUT | 2: // EP2 bulk OUT: CDC
            if (U_TOG_OK)       // Ignore if data toggle not synchronized
            {
                saved_es = ES;
                ES = 0;
#if CDC_EP2_MANUAL_TOG
                // 仅接受同步 OUT 后推进，包括零长包；重试包不推进或再次入队。
                UEP2_CTRL ^= bUEP_R_TOG;
#endif
                // Get current RX channel by OUT endpoint TOG bit
                channel  = !((UEP2_CTRL & bUEP_R_TOG) >> 7);
                
                if (USB_RX_LEN != 0U)
                {
#if CDC_DIAGNOSTICS
                    CDC_DIAG_ADD(usb_out_bytes, USB_RX_LEN);
#endif
                    CDC.tx_length[channel] = USB_RX_LEN;
                    CDC_SetTxState(channel, 1);

                    if (!CDC_GetTxState(channel ^ 1))
                    {
                        // 只有 UART 空闲时设置活动通道；排队包不能改动发送索引。
                        CDC_SetTxChannel(channel);
                        CDC.tx_index = 0U;
                        SBUF = (UIS_EP2_OUT_BUF(channel))[CDC.tx_index++];
#if CDC_DIAGNOSTICS
                        CDC_DIAG_INC(uart_tx_bytes);
#endif
                    }
                }
                // 下一次 OUT 的 DMA 通道仍忙时 NAK，包括零长包推进 TOG 的情况。
                if (!CDC_GetTxState(channel ^ 1))
                {
                    UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
                }
                else
                {
                    UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_NAK;
                }
                ES = saved_es;
            }
            break;
        
        case UIS_TOKEN_IN | 2: // EP2 bulk IN: CDC
            saved_es = ES;
            ES = 0;
            CDC_IN_DONE_MARK();
#if CDC_EP2_MANUAL_TOG
            // 成功 IN 的完成中断才推进；NAK/未获 ACK 的重试保持原槽位和 PID。
            UEP2_CTRL ^= bUEP_T_TOG;
#endif
#if CDC_DIAGNOSTICS
            // 交接前长度仍属于刚完成的包；UART 此时不能重装它。
            CDC_DIAG_ADD(usb_in_bytes, UEP2_T_LEN);
#endif
            // Get current TX channel by IN endpoint TOG bit
            channel  = ((UEP2_CTRL & bUEP_T_TOG) >> 6);

#if CDC_IN_HANDOFF_DIAG
            // 已提交的通道与聚包通道相反；成功 IN 后硬件 TOG 应指向聚包通道。
            // 只记录失配，不纠正 TOG、不重发，也不掩盖本次传输计数。
            if (CDC_GetRxChannel() != channel) {
#if CDC_IN_SNAPSHOT_DIAG
                if (!(CDC.flags & (1U << CDC_RX_CHANNEL_MISMATCH_BIT))) {
                    // UART 已屏蔽；在改队列前只锁存首个失配，不改硬件 TOG。
                    CDC.in_snapshot[0] = UEP2_CTRL;
                    CDC.in_snapshot[1] = UEP2_T_LEN;
                    CDC.in_snapshot[2] = CDC.flags;
                    CDC.in_snapshot[3] = CDC.rx_length;
                    CDC.in_snapshot[4] = CDC.uart_rx_bytes[0];
                    CDC.in_snapshot[5] = CDC.uart_rx_bytes[1];
                    CDC.in_snapshot[6] = CDC.usb_in_bytes[0];
                    CDC.in_snapshot[7] = CDC.usb_in_bytes[1];
                }
#endif
                CDC.flags |= (1U << CDC_RX_CHANNEL_MISMATCH_BIT);
            }
#endif

            CDC_SetRxChannel(channel);
            CDC_SetRxState(channel ^ 1, 0); // Clear the other channel state

#if CDC_RX_RING
            // Stop exposing the completed slot while the FIFO fills the next.
            UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;
            ES = saved_es;
            CDC_RxPump();
            ES = 0;
#endif

            if(CDC_GetRxState(channel)
#if CDC_RX_BATCH
               && CDC.rx_length == CDC_EP_BUF_SIZE
#endif
            )
            {
//                UEP2_T_LEN = 0;    // Preload length must be cleared
                // Data ready on this channel
                UEP2_T_LEN = CDC.rx_length;
                UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
                CDC_IN_ARM_MARK();
                CDC.rx_length = 0;
                CDC_ToggleRxChannel();
            }
            else
            {
                UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK; // Default NAK
            }
            ES = saved_es;
#if CDC_RX_FIFO && !CDC_RX_RING
            CDC_RxDrain();
#endif
            break;
        
		case UIS_TOKEN_IN | 4: // EP4 interrupt IN
			UEP4_T_LEN = 0;
			UEP4_CTRL ^= bUEP_T_TOG;
			UEP4_CTRL = UEP4_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           // Default NAK
			break;
        
        case UIS_TOKEN_IN | 0: // endpoint0 IN
            switch (SetupReq)
            {
                case USB_GET_DESCRIPTOR:
                case 0x20:
                    len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen; // Transfer length this time
                    memcpy(Ep0Buffer, pDescr, len);                                 // Load IN data
                    SetupLen -= len;
                    pDescr += len;
                    UEP0_T_LEN = len;
                    UEP0_CTRL ^= bUEP_T_TOG; // Toggle DATA0/DATA1
                    break;
                case USB_SET_ADDRESS:
                    USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
                    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    break;
                default:
                    UEP0_T_LEN = 0; // Status stage or force ZLP to end control transfer
                    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    break;
            }
            break;

        case UIS_TOKEN_OUT | 0: // endpoint0 OUT
            len = USB_RX_LEN;
            if (SetupReq == 0x20) // SET_LINE_CODING: set UART parameters
            {
                if (U_TOG_OK)
                {
                    // 先验证并配置，成功才提交；拒绝时保留旧 LineCoding。
                    if (len != sizeof(LineCoding) || !Config_Uart0(Ep0Buffer))
                    {
                        UEP0_CTRL = UEP_R_RES_STALL | UEP_T_RES_STALL;
                        break;
                    }
                    memcpy(LineCoding, Ep0Buffer, sizeof(LineCoding));
                    UEP0_T_LEN = 0;
                    UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_ACK; // Prepare to send ZLP
                }
            }
            else if(SetupReq == 0x22) // SET_CONTROL_LINE_STATE 
            {
                if (U_TOG_OK)
                {
                    UEP0_T_LEN = 0;
                    UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_ACK; // Prepare to send ZLP
                }
            }
            else if (SetupReq == 0x09)
            {
                if (Ep0Buffer[0])
                {
                }
                else if (Ep0Buffer[0] == 0)
                {
                }
            }
            UEP0_CTRL ^= bUEP_R_TOG; // Toggle DATA0/DATA1
            break;

        case UIS_TOKEN_SETUP | 0: // SETUP transaction
            len = USB_RX_LEN;
            if (len == (sizeof(USB_SETUP_REQ)))
            {
                SetupLen = UsbSetupBuf->wLengthL;
                if (UsbSetupBuf->wLengthH)
                    SetupLen = 0xFF; // Limit transfer length
                len = 0;             // Default success, return ZLP
                SetupReq = UsbSetupBuf->bRequest;
                switch (UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK)
                {
                case USB_REQ_TYP_STANDARD:
                    switch (SetupReq) // Request code
                    {
                    case USB_GET_DESCRIPTOR:
                        switch (UsbSetupBuf->wValueH)
                        {
                        case USB_DESCR_TYP_DEVICE:               // Device descriptor
                            pDescr = DevDesc; // Select descriptor buffer
                            len = sizeof(DevDesc);
                            break;
                        case USB_DESCR_TYP_CONFIG:               // Configuration descriptor
                            pDescr = CfgDesc; // Select descriptor buffer
                            len = sizeof(CfgDesc);
                            break;
                        case USB_DESCR_TYP_STRING: // String descriptor
                            switch (UsbSetupBuf->wValueL)
                            {
                            case 0:
                                pDescr = (PUINT8)(&LangDescr[0]);
                                len = sizeof(LangDescr);
                                break;
                            case 1:
                                pDescr = (PUINT8)(&ManuInfo[0]);
                                len = sizeof(ManuInfo);
                                break;
                            case 2:
                                pDescr = (PUINT8)(&ProdInfo[0]);
                                len = sizeof(ProdInfo);
                                break;
                            case 3:
                                pDescr = (PUINT8)(&SerNumber[0]);
                                len = sizeof(SerNumber);
                                break;
                            case 4:
                                pDescr = (PUINT8)(&DAPInfo[0]);
                                len = sizeof(DAPInfo);
                                break;
                            case 5:
                                pDescr = (PUINT8)(&CDCInfo[0]);
                                len = sizeof(CDCInfo);
                                break;
                            // case 6:
                            //     pDescr = (PUINT8)(&CDCFunc[0]);
                            //     len = sizeof(CDCFunc);
                            //     break;
                            default:
                                len = 0xFF; // Unsupported string descriptor
                                break;
                            }
                            break;
                        case 15:
                            pDescr = (PUINT8)(&BOSDesc[0]);
                            len = sizeof(BOSDesc);
                            break;
                        default:
                            len = 0xff; // Unsupported descriptor type
                            break;
                        }
                        break;
                    case USB_SET_ADDRESS:
                        SetupLen = UsbSetupBuf->wValueL; // Store device address
                        break;
                    case USB_GET_CONFIGURATION:
                        Ep0Buffer[0] = UsbConfig;
                        if (SetupLen >= 1)
                        {
                            len = 1;
                        }
                        break;
                    case USB_SET_CONFIGURATION:
                        UsbConfig = UsbSetupBuf->wValueL;
                        if (UsbConfig)
                        {
                            Ready = 1; // Set when enumeration is done
                        }
                        break;
                    case 0x0A:
                        break;
                    case USB_CLEAR_FEATURE:                                                         // Clear Feature
                        if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) // Endpoint
                        {
                            switch (UsbSetupBuf->wIndexL)
                            {
                            case 0x82:
                                UEP2_CTRL = UEP2_CTRL & ~(bUEP_T_TOG | MASK_UEP_T_RES) | UEP_T_RES_NAK;
                                break;
                            case 0x81:
                                UEP1_CTRL = UEP1_CTRL & ~(bUEP_T_TOG | MASK_UEP_T_RES) | UEP_T_RES_NAK;
                                break;
                            case 0x02:
                                UEP2_CTRL = UEP2_CTRL & ~(bUEP_R_TOG | MASK_UEP_R_RES) | UEP_R_RES_ACK;
                                break;
                            default:
                                len = 0xFF; // Unsupported endpoint
                                break;
                            }
                        }
                        else
                        {
                            len = 0xFF; // Unsupported recipient
                        }
                        break;
                    case USB_SET_FEATURE: // 固定描述符不支持远程唤醒，仅保留端点 HALT。
                        if ((UsbSetupBuf->bRequestType & 0x1F) != 0x02 ||
                            UsbSetupBuf->wValueH || UsbSetupBuf->wValueL ||
                            UsbSetupBuf->wIndexH)
                        {
                            len = 0xFF;
                            break;
                        }
                        switch (UsbSetupBuf->wIndexL)
                        {
                        case 0x82:
                            UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL; /* Set EP2 IN STALL */
                            break;
                        case 0x02:
                            UEP2_CTRL = UEP2_CTRL & (~bUEP_R_TOG) | UEP_R_RES_STALL; /* Set EP2 OUT STALL */
                            break;
                        case 0x81:
                            UEP1_CTRL = UEP1_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL; /* Set EP1 IN STALL */
                            break;
                        default:
                            len = 0xFF;
                            break;
                        }
                        break;
                    case USB_GET_STATUS:
                        pDescr = (PUINT8)&USB_STATUS;
                        if (SetupLen >= 2)
                        {
                            len = 2;
                        }
                        else
                        {
                            len = SetupLen;
                        }
                        break;
                    default:
                        len = 0xff; // Operation failed
                        break;
                    }

                    break;
                case USB_REQ_TYP_CLASS: /* HID class request */
                    if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                    {
                        switch (SetupReq)
                        {
                        case 0x20: // SET_LINE_CODING
                            if (SetupLen != sizeof(LineCoding))
                                len = 0xFF; // CDC 参数必须完整为 7 字节。
                            break;
                        case 0x21: // GET_LINE_CODING
                            pDescr = LineCoding;
                            len = sizeof(LineCoding);
                            break;
                        case 0x22: // generates RS-232/V.24 style control signals
//                            log_printf("S \n");
                            break;
                        default:
                            len = 0xFF; /* Unsupported command */
                            break;
                        }
                    }
                    break;
                case USB_REQ_TYP_VENDOR:
                    if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        switch (SetupReq)
                        {
                        case 0x20: // GetReport
                            if (UsbSetupBuf->wIndexL == 0x07)
                            {
                                pDescr = WINDesc; // Select descriptor buffer
                                len = sizeof(WINDesc);
                            }
                            break;
                        default:
                            len = 0xFF; /* Unsupported command */
                            break;
                        }
                    }
                    break;
                default:
                    len = 0xFF;
                    break;
                }
                if (len != 0 && len != 0xFF)
                {
                    if (SetupLen > len)
                    {
                        SetupLen = len; // Clamp total transfer length
                    }
                    len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen; // Transfer length this time
                    memcpy(Ep0Buffer, pDescr, len);                                 // Load IN data
                    SetupLen -= len;
                    pDescr += len;
                }
            }
            else
            {
                len = 0xff; // Packet length error
            }
            if (len == 0xff)
            {
                SetupReq = 0xFF;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
            }
            else if (len <= THIS_ENDP0_SIZE) // Upload data or return ZLP in status stage
            {
                UEP0_T_LEN = len;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // Default DATA1, ACK
            }
            else
            {
                UEP0_T_LEN = 0;                                                      // Prepare ZLP in case host enters status stage early
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // Default DATA1, ACK
            }
            break;

        default:
            break;
        }
        UIF_TRANSFER = 0; // Clear interrupt flag
    }
    else if (UIF_BUS_RST) // USB bus reset (device mode)
    {
#if SWO_UART
        IE_UART1 = 0; U1REN = 0; // Stop old trace immediately; task resets ring/session.
#endif
        saved_es = ES;
        ES = 0;
        // DMA TOG 复位前丢弃旧 CDC 队列，不改变当前波特率设置。
        CDC.flags = 0U;
        CDC_IN_MARK_RESET();
        CDC.tx_length[0] = 0U;
        CDC.tx_length[1] = 0U;
        CDC.tx_index = 0U;
        CDC.rx_length = 0U;
#if CDC_RX_FIFO
#if CDC_RX_RING
        CDC_RxWrite = CDC_RxRead = 0U;
#else
        CDC_RxWrite = CDC_RxRead = CDC_RxCount = 0U;
#endif
#endif
#if CDC_DIAGNOSTICS
        CDC.rx_dropped[0] = 0U;
        CDC.rx_dropped[1] = 0U;
        CDC.usb_out_bytes[0] = CDC.usb_out_bytes[1] = 0U;
        CDC.uart_tx_bytes[0] = CDC.uart_tx_bytes[1] = 0U;
        CDC.uart_rx_bytes[0] = CDC.uart_rx_bytes[1] = 0U;
        CDC.usb_in_bytes[0] = CDC.usb_in_bytes[1] = 0U;
#endif
        RI = 0;
        TI = 0;
        UEP2_T_LEN = 0U;
        // 清理软件队列；正在运行的传输尽快退出，任务丢弃旧会话的响应。
        DAP_UsbEpoch++;
        DAP_TransferAbort_Flag = 1U;
        DapState[0] = 0U;
        DapState[1] = 0U;
        DAP_RequestLength[0] = 0U;
        DAP_RequestLength[1] = 0U;
        UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
#if !DAP_EP1_MANUAL_TOG
        UEP1_CTRL |= bUEP_AUTO_TOG;
#endif
        UEP2_CTRL = CDC_EP2_INIT_CTRL;
//        UEP3_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP4_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        USB_DEV_AD = 0x00;
        UIF_SUSPEND = 0;
        UIF_TRANSFER = 0;
        UIF_BUS_RST = 0; // Clear interrupt flag
        ES = saved_es;
    }
    else if (UIF_SUSPEND) // USB suspend/wakeup
    {
        UIF_SUSPEND = 0;
        if (USB_MIS_ST & bUMS_SUSPEND) // Suspended
        {
        }
    }
    else
    {
        // Unexpected interrupt (should not happen)
        USB_INT_FG = 0xFF; // Clear interrupt flags
    }
}

/******************************************************************************/
/*       Task 3 'usb_task'                                                   */
/******************************************************************************/
void Usb_Task (void) _task_ USB {
    USBDeviceInit();
    os_delete_task(USB);
//    while (1)
//    {
//        /* endless loop                         */
//        os_wait(K_TMO, 100, 0);            /* wait for timeout: 100 ticks            */
//    }
}
