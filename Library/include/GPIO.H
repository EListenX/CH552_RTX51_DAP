#ifndef CH552_LIB_GPIO_H
#define CH552_LIB_GPIO_H

#define GPIO_INTERRUPT   1 
  
/*******************************************************************************
* Function Name  : Port1Cfg()
* Description    : 端口1配置
* Input          : Mode  0 = 浮空输入，无上拉
                         1 = 推挽输入输出
                         2 = 开漏输入输出，无上拉
                         3 = 类51模式，开漏输入输出，有上拉，内部电路可以加速由低到高的电平爬升		
                   ,UINT8 Pin	(0-7)											 
* Output         : None
* Return         : None
*******************************************************************************/
// Mode 和 Pin 各求值一次；Pin 的有效范围为 0..7，不提供函数地址。
#define CH552_PORT_CONFIG(oc_reg, dir_reg, mode, pin) do { \
    UINT8 ch552_gpio_mode_ = (UINT8)(mode); \
    UINT8 ch552_gpio_pin_ = (UINT8)(pin); \
    UINT8 ch552_gpio_mask_ = (UINT8)(1U << ch552_gpio_pin_); \
    if (ch552_gpio_mode_ <= 3U) { \
        if (ch552_gpio_mode_ & 2U) (oc_reg) |= ch552_gpio_mask_; \
        else (oc_reg) &= ~ch552_gpio_mask_; \
        if (ch552_gpio_mode_ & 1U) (dir_reg) |= ch552_gpio_mask_; \
        else (dir_reg) &= ~ch552_gpio_mask_; \
    } \
} while (0)
#define Port1Cfg(mode, pin) CH552_PORT_CONFIG(P1_MOD_OC, P1_DIR_PU, mode, pin)
#define Port3Cfg(mode, pin) CH552_PORT_CONFIG(P3_MOD_OC, P3_DIR_PU, mode, pin)

/*******************************************************************************
* Function Name  : GPIOInterruptCfg()
* Description    : GPIO中断配置
* Input          : None									 
* Output         : None
* Return         : None
*******************************************************************************/
#define GPIOInterruptCfg() do { \
    GPIO_IE &= ~bIE_IO_EDGE; /* 高/低电平触发 */ \
    /*    GPIO_IE |= bIE_IO_EDGE;                                                  //上升/下降触发 */ \
    /*    GPIO_IE |= bIE_RXD1_LO;                                                     //RXD1低电平或下降沿触发 */ \
    GPIO_IE |= bIE_P1_5_LO | bIE_P1_4_LO | bIE_P1_3_LO | bIE_RST_HI; \
    /* P15\P14\P13低电平触发；RST高电平触发 */ \
    /*    GPIO_IE |= bIE_P3_1_LO;                                                     //P31低电平或下降沿触发 */ \
    /*    GPIO_IE |= bIE_RXD0_LO;                                                     //RXD0低电平或下降沿触发 */ \
    IE_GPIO = 1; /* GPIO中断开启 */ \
} while (0)

#endif
