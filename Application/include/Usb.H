/********************************** (C) COPYRIGHT *******************************
* File Name          : Usb.H
* Author             : EListen
* Version            : V1.00
* Date               : 2022-07-31
* Description        : 
*******************************************************************************/
#ifndef USB_H
#define USB_H

#define Fullspeed          1
#define WINUSB             1
#define THIS_ENDP0_SIZE    64
//#define DAP_PACKET_COUNT   2

#define USB_DESCR_TYP_IAD      0x0B
#define USB_DESCR_TYP_BOS      0x0F
#define SET_LINE_CODING        0x20        // Configures DTE rate, stop-bits, parity, and number-of-character
#define GET_LINE_CODING        0x21        // This request allows the host to find out the currently configured line coding.
#define SET_CONTROL_LINE_STATE 0x22        // This request generates RS-232/V.24 style control signals.

#define UIS_TOKEN_SETUP_EP0  (UIS_TOKEN_SETUP | 0)

#define UIS_TOKEN_IN_EP0  (UIS_TOKEN_IN | 0)
#define UIS_TOKEN_IN_EP1  (UIS_TOKEN_IN | 1)
#define UIS_TOKEN_IN_EP2  (UIS_TOKEN_IN | 2)
#define UIS_TOKEN_IN_EP3  (UIS_TOKEN_IN | 3)
#define UIS_TOKEN_IN_EP4  (UIS_TOKEN_IN | 4)

#define UIS_TOKEN_OUT_EP0  (UIS_TOKEN_OUT | 0)
#define UIS_TOKEN_OUT_EP1  (UIS_TOKEN_OUT | 1)
#define UIS_TOKEN_OUT_EP2  (UIS_TOKEN_OUT | 2)
#define UIS_TOKEN_OUT_EP3  (UIS_TOKEN_OUT | 3)
#define UIS_TOKEN_OUT_EP4  (UIS_TOKEN_OUT | 4)

#define UIS_EP1_OUT_BUF(a)    (&Ep1Buffer + (0 + a) * THIS_ENDP0_SIZE)
#define UIS_EP1_IN_BUF(a)     (&Ep1Buffer + (2 + a) * THIS_ENDP0_SIZE)

/* Keep the legacy address expression available for paired firmware tests. */
#ifndef CDC_EP2_XDATA
#define CDC_EP2_XDATA 0
#endif
#if CDC_EP2_XDATA != 0 && CDC_EP2_XDATA != 1
#error CDC_EP2_XDATA must be 0 or 1
#endif
#if CDC_EP2_XDATA
#define UIS_EP2_OUT_BUF(a)    ((PUINT8X)Ep2Buffer + (a) * THIS_ENDP0_SIZE)
#define UIS_EP2_IN_BUF(a)     ((PUINT8X)Ep2Buffer + (2 + (a)) * THIS_ENDP0_SIZE)
#else
#define UIS_EP2_OUT_BUF(a)    (&Ep2Buffer + (0 + a) * THIS_ENDP0_SIZE)
#define UIS_EP2_IN_BUF(a)     (&Ep2Buffer + (2 + a) * THIS_ENDP0_SIZE)
#endif

#define UsbSetupBuf ((PUSB_SETUP_REQ)Ep0Buffer)

extern UINT8X Ep0Buffer[];  //???0 OUT&IN??????????????????
extern UINT8X Ep4Buffer[];  //???4 OUT&IN?????????????????? Not Change!!!!!!
extern UINT8X Ep1Buffer[];  //???1 OUT&IN?????????????????? Not Change!!!!!!
extern UINT8X Ep2Buffer[];  //???2 OUT&IN?????????????????? Not Change!!!!!!

#endif
/* END OF FILE */
