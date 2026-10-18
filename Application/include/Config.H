/********************************** (C) COPYRIGHT *******************************
 * File Name          : Config.H
 * Author             : EListen
 * Version            : V1.10
 * Description        : RTX-51 Tiny task definitions and scheduling parameters
 *******************************************************************************/
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <rtx51tny.h> /* RTX-51 tiny functions & defines */

/*-----------------------------------------------------------------------------
 * RTX-51 Tiny Task IDs
 *
 * Task IDs must be contiguous integers starting from 0.
 * Task 0 (TASK_INIT) is executed first upon os_start_system().
 *----------------------------------------------------------------------------*/
#define TASK_INIT        0   /* Task 0: System initialization & task spawning */
#define TASK_DAP         1   /* Task 1: CMSIS-DAP protocol engine */
#define TASK_USB         2   /* Task 2: USB device state & event management */
#define TASK_UART        3   /* Task 3: CDC UART bridge transmission */
#define TASK_KEYREAD     5   /* Task 5: Key / button polling (reserved) */
#define TASK_GET_ESC     6   /* Task 6: Escape sequence parser (reserved) */

/*-----------------------------------------------------------------------------
 * Legacy Task Name Aliases (Backward Compatibility)
 *----------------------------------------------------------------------------*/
#define INIT             TASK_INIT
#define DAP              TASK_DAP
#define USB              TASK_USB
#define UART             TASK_UART
#define KEYREAD          TASK_KEYREAD
#define GET_ESC          TASK_GET_ESC

#endif /* __CONFIG_H__ */
/* END OF FILE */
