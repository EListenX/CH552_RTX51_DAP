/********************************** (C) COPYRIGHT *******************************
* File Name          : Usb_Config.H
* Author             : EListen
* Version            : V1.00
* Date               : 2022-07-31
* Description        : USB device descriptor and config (DAP + CDC)
*******************************************************************************/
#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#define WINUSB             1

#if (WINUSB == 1)
UINT8C DevDesc[] =
{
   0x12,                            // bLength: descriptor size
   USB_DESCR_TYP_DEVICE,            // bDescriptorType: Device descriptor
   0x10, 0x02,                      // bcdUSB: USB version (0x0210 = USB 2.1)
   0xEF,                            // bDeviceClass: Misc device
   0x02,                            // bDeviceSubClass
   0x01,                            // bDeviceProtocol: IAD
   THIS_ENDP0_SIZE,                 // bMaxPacketSize0: EP0 max packet
   0x28, 0x0D,                      // idVendor
   0x04, 0x02,                      // idProduct
   0x00, 0x01,                      // bcdDevice: device version
   0x01,                            // iManufacturer: string index
   0x02,                            // iProduct: string index
   0x03,                            // iSerialNumber: string index
   0x01                             // bNumConfigurations
};
UINT8C CfgDesc[] =
{
    // Configuration descriptor
    0x09,                            // bLength
    USB_DESCR_TYP_CONFIG,            // bDescriptorType: Configuration
    0x62, 0x00,                      // wTotalLength
    0x03,                            // bNumInterfaces
    0x01,                            // bConfigurationValue
    0x00,                            // iConfiguration: string index
    0x80,                            // bmAttributes: bus powered
    0xfa,                            // bMaxPower: 500mA

    // DAP interface descriptor
    0x09,                            // bLength
    USB_DESCR_TYP_INTERF,            // bDescriptorType: Interface
    0x00,                            // bInterfaceNumber
    0x00,                            // bAlternateSetting
    0x02,                            // bNumEndpoints (excl. EP0)
    0xff,                            // bInterfaceClass: vendor
    0x00,                            // bInterfaceSubClass
    0x00,                            // bInterfaceProtocol
    0x04,                            // iInterface: string index

    // DAP endpoint OUT descriptor
    0x07,                            // bLength
    USB_DESCR_TYP_ENDP,              // bDescriptorType: Endpoint
    0x01,                            // bEndpointAddress
    0x02,                            // bmAttributes: bulk
    0x40, 0x00,                      // wMaxPacketSize
    0x00,                            // bInterval

    // DAP endpoint IN descriptor
    0x07,                            // bLength
    USB_DESCR_TYP_ENDP,              // bDescriptorType: Endpoint
    0x81,                            // bEndpointAddress
    0x02,                            // bmAttributes: bulk
    0x40, 0x00,                      // wMaxPacketSize
    0x00,                            // bInterval

    // Interface Association: CDC
    0x08,                            // bLength
    USB_DESCR_TYP_IAD,               // bDescriptorType: IAD
    0x01,                            // bFirstInterface
    0x02,                            // bInterfaceCount
    0x02,                            // bFunctionClass: CDC
    0x02,                            // bFunctionSubClass
    0x01,                            // bFunctionProtocol
    0x05,                            // iFunction: string index

    // CDC data interface descriptor
    0x09,                            // bLength
    USB_DESCR_TYP_INTERF,            // bDescriptorType: Interface
    0x01,                            // bInterfaceNumber
    0x00,                            // bAlternateSetting
    0x02,                            // bNumEndpoints (excl. EP0)
    0x0A,                            // bInterfaceClass: CDC data
    0x00,                            // bInterfaceSubClass
    0x00,                            // bInterfaceProtocol
    0x00,                            // iInterface

    // CDC data endpoint OUT
    0x07,                            // bLength
    USB_DESCR_TYP_ENDP,              // bDescriptorType: Endpoint
    0x02,                            // bEndpointAddress
    0x02,                            // bmAttributes: bulk
    0x40, 0x00,                      // wMaxPacketSize
    0x01,                            // bInterval

    // CDC data endpoint IN
    0x07,                            // bLength
    USB_DESCR_TYP_ENDP,              // bDescriptorType: Endpoint
    0x82,                            // bEndpointAddress
    0x02,                            // bmAttributes: bulk
    0x40, 0x00,                      // wMaxPacketSize
    0x01,                            // bInterval

    // CDC control interface descriptor
    0x09,                            // bLength
    USB_DESCR_TYP_INTERF,            // bDescriptorType: Interface
    0x02,                            // bInterfaceNumber
    0x00,                            // bAlternateSetting
    0x01,                            // bNumEndpoints (excl. EP0)
    0x02,                            // bInterfaceClass: CDC
    0x02,                            // bInterfaceSubClass
    0x01,                            // bInterfaceProtocol
    0x05,                            // iInterface: string index

    // CDC function: Header
    0x05,                            // bFunctionLength
    0x24,                            // bDescriptorType: CS_INTERFACE
    0x00,                            // bDescriptorSubtype: Header
    0x10, 0x01,                      // bcdCDC: CDC version

    // CDC function: Call Management
    0x05,                            // bFunctionLength
    0x24,                            // bDescriptorType
    0x01,                            // bDescriptorSubtype: Call Management
    0x00,                            // bmCapabilities
    0x01,                            // bDataInterface

    // CDC function: Abstract Control
    0x04,                            // bFunctionLength
    0x24,                            // bDescriptorType
    0x02,                            // bDescriptorSubtype: Abstract Control
    0x02,                            // bmCapabilities: Line Coding, Control Line, Serial State

    // CDC function: Union
    0x05,                            // bFunctionLength
    0x24,                            // bDescriptorType
    0x06,                            // bDescriptorSubtype: Union
    0x00,                            // bControlInterface
    0x01,                            // bSubordinateInterface0

    // Endpoint descriptor: Notification (IN, EP4)
    0x07,                            // bLength
    USB_DESCR_TYP_ENDP,              // bDescriptorType: Endpoint
    0x84,                            // bEndpointAddress: IN, EP4
    0x03,                            // bmAttributes: interrupt
    0x08, 0x00,                      // wMaxPacketSize: 8 bytes
    0xFF,                            // bInterval
};
#else
#Err no USB interface define
#endif

// CDC Set_Line_Coding default
UINT8I LineCoding[] =
{
   0x00, 0xe1, 0x00, 0x00,          /* dwDTERate: 57600 default */
   0x00,                            /* bCharFormat: 1 stop bit */
   0x00,                            /* bParityType: None */
   0x08                             /* bDataBits: 8 */
};

/* String descriptors */
// Language ID (en-US)
UINT8C LangDescr[] =
{
   0x04,                            /* bLength */
   USB_DESCR_TYP_STRING,            /* bDescriptorType */
   0x09,                            /* wLANGID[0] */
   0x04                             /* wLANGID[1] */
};
// Manufacturer: ARM
UINT8C ManuInfo[] =
{
   0x08,                            /* bLength */
   USB_DESCR_TYP_STRING,            /* bDescriptorType */
   'A', 0x00,                       /* wcChar0 */
   'R', 0x00,                       /* wcChar1 */
   'M', 0x00,                       /* wcChar2 */
};
// Product: EListen CMSIS-DAP v2
UINT8C ProdInfo[] =
{
 0x2a, USB_DESCR_TYP_STRING,
 'E', 0x00,
 'L', 0x00,
 'i', 0x00,
 's', 0x00,
 't', 0x00,
 'e', 0x00,
 'n', 0x00,
 ' ', 0x00,
 'C', 0x00,
 'M', 0x00,
 'S', 0x00,
 'I', 0x00,
 'S', 0x00,
 '-', 0x00,
 'D', 0x00,
 'A', 0x00,
 'P', 0x00,
 ' ', 0x00,
 'v', 0x00,
 '2', 0x00,
};
// DAP interface: EListen CMSIS-DAP v2
UINT8C DAPInfo[] =
{
 0x2a, USB_DESCR_TYP_STRING,
 'E', 0x00,
 'L', 0x00,
 'i', 0x00,
 's', 0x00,
 't', 0x00,
 'e', 0x00,
 'n', 0x00,
 ' ', 0x00,
 'C', 0x00,
 'M', 0x00,
 'S', 0x00,
 'I', 0x00,
 'S', 0x00,
 '-', 0x00,
 'D', 0x00,
 'A', 0x00,
 'P', 0x00,
 ' ', 0x00,
 'v', 0x00,
 '2', 0x00,
};
// 唯一序列号描述符由 Usb.C 中的 USBSerialInit 在枚举前生成。


// CDC interface string
UINT8C CDCInfo[] =
{
   0x18,                            /* bLength */
   USB_DESCR_TYP_STRING,            /* bDescriptorType */
   'D', 0x00,                       /* wcChar0 */
   'a', 0x00,                       /* wcChar1 */
   'p', 0x00,                       /* wcChar2 */
   'l', 0x00,                       /* wcChar3 */
   'i', 0x00,                       /* wcChar4 */
   'n', 0x00,                       /* wcChar5 */
   'k', 0x00,                       /* wcChar6 */
   '-', 0x00,                       /* wcChar7 */
   'C', 0x00,                       /* wcChar8 */
   'D', 0x00,                       /* wcChar9 */
   'C', 0x00,                       /* wcChar10 */
};

// CDC function string (commented out)
// UINT8C CDCFunc[] =
// {
//    0x1C,                            /* bLength */
//    USB_DESCR_TYP_STRING,            /* bDescriptorType */
//    'C', 0x00, 'D', 0x00, 'C', 0x00, '-', 0x00,
//    'C', 0x00, 'o', 0x00, 'm', 0x00, 'p', 0x00,
//    'o', 0x00, 's', 0x00, 'i', 0x00, 't', 0x00, 'e', 0x00,
// };

// BOS device capability descriptor
UINT8C BOSDesc[] =
{
   0x05,                                      /* bLength */
   USB_DESCR_TYP_BOS,                         /* bDescriptorType: BOS */
   0x28, 0x00,                                /* wTotalLength */
   0x02,                                      /* bNumDeviceCaps */
   0x07, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00,  /* USB 2.0 extension */
   0x1C,                                      /* bLength: platform descriptor */
   0x10,                                      /* bDescriptorType: device capability */
   0x05,                                      /* bDevCapabilityType: platform */
   0x00,                                      /* bReserved */
   0xDF, 0x60, 0xDD, 0xD8,                    /* PlatformCapabilityUUID: Microsoft OS */
   0x89, 0x45, 0xC7, 0x4C,
   0x9C, 0xD2, 0x65, 0x9D,
   0x9E, 0x64, 0x8A, 0x9F,
   0x00, 0x00, 0x03, 0x06,                    /* dwWindowsVersion: >= Win 8.1 */
   0xAA, 0x00,                                /* wDescriptorSetTotalLength */
   0x20,                                      /* bVendorCode */
   0x00                                       /* bAltEnumCode */
};

// WINUSB WCID 2.0 descriptor
UINT8C WINDesc[] =
{
   // WCID20 header
   0x0a, 0x00,                                       /* wLength */
   0x00, 0x00,                                       /* wDescriptorType */
   0x00, 0x00, 0x03, 0x06,                           /* dwWindowsVersion */
   0xaa, 0x00,                                       /* wDescriptorSetTotalLength */

   // WCID20 subset header
   0x08, 0x00,                                       /* wLength */
   0x02, 0x00,                                       /* wDescriptorType */
   0x00,                                             /* bFirstInterface */
   0x00,                                             /* bReserved */
   0xa0, 0x00,                                       /* wSubsetLength */

   // WCID20 function ID
   0x14, 0x00,                                       /* wLength */
   0x03, 0x00,                                       /* wDescriptorType */
   /* WINUSB */
   'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,         /* cCID_8 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* cSubCID_8 */

   // WCID20 property section
   0x84, 0x00,                                       /* wLength */
   0x04, 0x00,                                       /* wDescriptorType */
   0x07, 0x00,                                       /* wPropertyDataType */
   0x2a, 0x00,                                       /* wPropertyNameLength */
   /* DeviceInterfaceGUIDs */
   'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00,       /* wcPropertyName */
   'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00,
   't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
   'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00,
   'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00,
   0x00, 0x00,
   0x50, 0x00,                                       /* wPropertyDataLength */
   /* {CDB3B5AD-293B-4663-AA36-1AAE46463776} */
   '{', 0x00, 'C', 0x00, 'D', 0x00, 'B', 0x00,       /* wcPropertyData: GUID */
   '3', 0x00, 'B', 0x00, '5', 0x00, 'A', 0x00,
   'D', 0x00, '-', 0x00, '2', 0x00, '9', 0x00,
   '3', 0x00, 'B', 0x00, '-', 0x00, '4', 0x00,
   '6', 0x00, '6', 0x00, '3', 0x00, '-', 0x00,
   'A', 0x00, 'A', 0x00, '3', 0x00, '6', 0x00,
   '-', 0x00, '1', 0x00, 'A', 0x00, 'A', 0x00,
   'E', 0x00, '4', 0x00, '6', 0x00, '4', 0x00,
   '6', 0x00, '3', 0x00, '7', 0x00, '7', 0x00,
   '6', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,
};

#endif
/* END OF FILE */
