
/********************************** (C) COPYRIGHT *******************************
 * File Name          : GPIO.C
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2017/01/20
 * Description        : CH552 IO 设置接口函数和GPIO中断函数
 *******************************************************************************/

#include "CH552.H"
#include "Debug.H"
#include "GPIO.H"
#include "stdio.h"

#pragma NOAREGS

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
// Port1Cfg 已改为头文件寄存器宏。

/*******************************************************************************
* Function Name  : Port3Cfg()
* Description    : 端口3配置
* Input          : Mode  0 = 浮空输入，无上拉
                         1 = 推挽输入输出
                         2 = 开漏输入输出，无上拉
                         3 = 类51模式，开漏输入输出，有上拉，内部电路可以加速由低到高的电平爬升
                   ,UINT8 Pin	(0-7)
* Output         : None
* Return         : None
*******************************************************************************/
// Port3Cfg 已改为头文件寄存器宏。

/*******************************************************************************
 * Function Name  : GPIOInterruptCfg()
 * Description    : GPIO中断配置
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
// GPIOInterruptCfg 已改为头文件寄存器宏，保留上述接口说明。

#ifdef GPIO_INTERRUPT
/*******************************************************************************
 * Function Name  : GPIOInterrupt(void)
 * Description    : GPIO 中断服务程序
 *******************************************************************************/
void GPIOInterrupt(void) interrupt INT_NO_GPIO using 1 // GPIO中断服务程序,使用寄存器组1
{
#if (DE_PRINTF != 0)
    printf("GPIO_STATUS: %02x\n", (UINT16)(PIN_FUNC & bIO_INT_ACT));
#endif
}
#endif
