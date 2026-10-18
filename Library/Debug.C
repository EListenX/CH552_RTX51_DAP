/********************************** (C) COPYRIGHT *******************************
* File Name          : Debug.C
* Author             : WCH
* Version            : V1.6
* Date               : 2021/12/15
* Description        : CH552 DEBUG Interface
                     CH552主频修改、延时函数定义
                     串口0和串口1初始化
                     串口0和串口1的收发子函数
                     看门狗初始化
*******************************************************************************/

#include "CH552.H"
#include "Debug.H"

/*******************************************************************************
* Function Name  : CfgFsys( )
* Description    : CH552时钟选择和配置函数,默认使用Fsys 6MHz，FREQ_SYS可以通过
                   CLOCK_CFG配置得到，公式如下：
                   Fsys = (Fosc * 4/(CLOCK_CFG & MASK_SYS_CK_SEL);具体时钟需要自己配置
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void CfgFsys()
{
#if OSC_EN_XT
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    CLOCK_CFG |= bOSC_EN_XT; // 使能外部晶振
    mDelaymS(10);
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    CLOCK_CFG &= ~bOSC_EN_INT; // 关闭内部晶振
    SAFE_MOD = 0x00;
#endif
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
#if FREQ_SYS == 24000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x06; // 24MHz
#endif
#if FREQ_SYS == 16000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x05; // 16MHz
#endif
#if FREQ_SYS == 12000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x04; // 12MHz
#endif
#if FREQ_SYS == 6000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x03; // 6MHz
#endif
#if FREQ_SYS == 3000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x02; // 3MHz
#endif
#if FREQ_SYS == 750000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x01; // 750KHz
#endif
#if FREQ_SYS == 187500
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x00; // 187.5KHz
#endif
    SAFE_MOD = 0x00;
}

/*******************************************************************************
 * Function Name  : mDelayus(UNIT16 n)
 * Description    : us延时函数
 * Input          : UNIT16 n
 * Output         : None
 * Return         : None
 *******************************************************************************/
void mDelayuS(UINT16 n) // 以uS为单位延时
{
#ifdef FREQ_SYS
#if FREQ_SYS <= 6000000
    n >>= 2;
#endif
#if FREQ_SYS <= 3000000
    n >>= 2;
#endif
#if FREQ_SYS <= 750000
    n >>= 4;
#endif
#endif
    while (n)
    {               // total = 12~13 Fsys cycles, 1uS @Fsys=12MHz
        ++SAFE_MOD; // 2 Fsys cycles, for higher Fsys, add operation here
#ifdef FREQ_SYS
#if FREQ_SYS >= 14000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 16000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 18000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 20000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 22000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 24000000
        ++SAFE_MOD;
#endif
#endif
        --n;
    }
}

/*******************************************************************************
 * Function Name  : mDelayms(UNIT16 n)
 * Description    : ms延时函数
 * Input          : UNIT16 n
 * Output         : None
 * Return         : None
 *******************************************************************************/
void mDelaymS(UINT16 n) // 以mS为单位延时
{
    while (n)
    {
#ifdef DELAY_MS_HW
        while ((TKEY_CTRL & bTKC_IF) == 0)
            ;
        while (TKEY_CTRL & bTKC_IF)
            ;
#else
        mDelayuS(1000);
#endif
        --n;
    }
}

/*******************************************************************************
 * Function Name  : CH552UART0Alter()
 * Description    : CH552串口0引脚映射,串口映射到P1.2和P1.3
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
// CH552UART0Alter 已改为头文件寄存器宏，保留上述接口说明。

/*******************************************************************************
* Function Name  : mInitSTDIO()
* Description    : CH552串口0初始化,默认使用T1作UART0的波特率发生器,也可以使用T2
                   作为波特率发生器
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void mInitSTDIO()
{
    // 固定配置在编译期完成取整，避免未使用的旧初始化也引入长整数除法库。
    // 与原先先取十分位再四舍五入完全一致，不改变 Timer1 初始化结果。

    SM0 = 0;
    SM1 = 1;
    SM2 = 0; // 串口0使用模式1
             // 使用Timer1作为波特率发生器
    RCLK = 0; // UART0接收时钟
    TCLK = 0; // UART0发送时钟
    PCON |= SMOD;

    TMOD = TMOD & ~bT1_GATE & ~bT1_CT & ~MASK_T1_MOD | bT1_M1; // 0X20，Timer1作为8位自动重载定时器
    T2MOD = T2MOD | bTMR_CLK | bT1_CLK;                        // Timer1时钟选择
    TH1 = 0 - ((10UL * FREQ_SYS / UART0_BUAD / 16UL + 5UL) / 10UL); // 原取整规则，编译期常量。
    TR1 = 1;                                                   // 启动定时器1
    TI = 1;
    REN = 1; // 串口0接收使能
}

/*******************************************************************************
* Function Name  : UART1Setup()
* Description    : CH554串口1初始化
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void UART1Init()
{
	U1SM0 = 0;                                                                   //UART1选择8位数据位
	U1SMOD = 1;                                                                  //快速模式
	U1REN = 1;                                                                   //使能接收
	SBAUD1 = 0 - FREQ_SYS/16/UART1_BUAD;
	U1TI = 0;
//#if UART1_PINMAP	
//    PIN_FUNC |= bUART1_PIN_X;                                                   //映射到P34(R)、P32(T)
//#endif

//#if UART1_INTERRUPT                                                            //开启中断使能
//	IE_UART1 = 1;	
//	EA = 1;
//#endif	
}

/*******************************************************************************
 * Function Name  : CH552UART0RcvByte()
 * Description    : CH552UART0接收一个字节
 * Input          : None
 * Output         : None
 * Return         : SBUF
 *******************************************************************************/
UINT8 CH552UART0RcvByte()
{
    while (RI == 0)
        ; // 查询接收，中断方式可不用
    RI = 0;
    return SBUF;
}

/*******************************************************************************
 * Function Name  : CH552UART0SendByte(UINT8 SendDat)
 * Description    : CH552UART0发送一个字节
 * Input          : UINT8 SendDat；要发送的数据
 * Output         : None
 * Return         : None
 *******************************************************************************/
void CH552UART0SendByte(UINT8 SendDat)
{
    SBUF = SendDat; // 查询发送，中断方式可不用下面2条语句,但发送前需TI=0
    while (TI == 0)
        ;
    TI = 0;
}

/*******************************************************************************
* Function Name  : CH552WDTModeSelect(UINT8 mode)
* Description    : CH552看门狗模式选择
* Input          : UINT8 mode
                   0  timer
                   1  watchDog
* Output         : None
* Return         : None
*******************************************************************************/
void CH552WDTModeSelect(UINT8 mode)
{
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xaa; // 进入安全模式
    if (mode)
    {
        GLOBAL_CFG |= bWDOG_EN; // 启动看门狗复位
    }
    else
        GLOBAL_CFG &= ~bWDOG_EN; // 启动看门狗仅仅作为定时器
    SAFE_MOD = 0x00;             // 退出安全模式
    WDOG_COUNT = 0;              // 看门狗赋初值
}

/*******************************************************************************
* Function Name  : CH552WDTFeed(UINT8 tim)
* Description    : CH552看门狗定时时间设置
* Input          : UINT8 tim 看门狗复位时间设置
                   00H(6MHz)=2.8s
                   80H(6MHz)=1.4s
* Output         : None
* Return         : None
*******************************************************************************/
// CH552WDTFeed 已改为头文件寄存器宏，保留上述接口说明。

// /*******************************************************************************
// * Function Name  : CH552SoftReset()
// * Description    : CH552软复位
// * Input          : None
// * Output         : None
// * Return         : None
// *******************************************************************************/
// void CH552SoftReset( )
// {
//     SAFE_MOD = 0x55;
//     SAFE_MOD = 0xAA;
//     GLOBAL_CFG	|=bSW_RESET;
// }

// /*******************************************************************************
// * Function Name  : CH552USBDevWakeup()
// * Description    : CH552设备模式唤醒主机，发送K信号
// * Input          : None
// * Output         : None
// * Return         : None
// *******************************************************************************/
// void CH552USBDevWakeup( )
// {
// #ifdef Fullspeed
//	UDEV_CTRL |= bUD_LOW_SPEED;
//	mDelaymS(2);
//	UDEV_CTRL &= ~bUD_LOW_SPEED;
// #else
//	UDEV_CTRL &= ~bUD_LOW_SPEED;
//	mDelaymS(2);
//	UDEV_CTRL |= bUD_LOW_SPEED;
// #endif
// }
