#ifndef CH55X_SWO_H
#define CH55X_SWO_H
#include "DAP.H"

#if SWO_UART
/* Task owns configuration/read index; UART1 ISR owns write index.
 * USB reset gates reception immediately; the DAP task resets the session.
 */
extern void SWO_Reset(void);
extern UINT8 SWO_Command(UINT8 command, const UINT8 xdata *req,
                         UINT8 xdata *resp, UINT8 capacity) DAP_SWO_MEMORY;
#define SWO_USB_RESET() do { IE_UART1 = 0; U1REN = 0; } while (0)
#else
#define SWO_USB_RESET() do { } while (0)
#endif
#endif
