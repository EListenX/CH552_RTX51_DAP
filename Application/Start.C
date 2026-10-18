
/********************************** (C) COPYRIGHT *******************************
 * File              : Start.C
 * Author            : WCH
 * Version           : V1.1
 * Date              : 2017/11/18
 * Description       : RTX-51 Tiny application entry
 * Features          : 1. System init
 *                    2. Create DAP, USB, UART tasks
 *******************************************************************************/
#include "CH552.H"
#include "Debug.H"
//#include "GPIO.H"
#include "stdio.h"
#include <string.h>
#include "Config.H"
#include "Uart.H"

#pragma NOAREGS

/******************************************************************************/
/* Task 0 - Init task (required by RTX-51 Tiny)                              */
/******************************************************************************/
void init(void) _task_ INIT
{
    CfgFsys();                // Configure system clock
    mDelaymS(20);             // Wait for clock stable
//    mInitSTDIO();
//    InitUart0(57600);
//    UART1Init();
    EA = 1;                   // Enable global interrupt
    os_create_task(DAP);      // Task 1: DAP protocol
    os_create_task(USB);      // Task 2: USB
    os_create_task(UART);     // Task 3: UART

    os_delete_task(INIT);
}

