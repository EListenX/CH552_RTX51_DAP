#ifndef CH55X_DAP_SPI_H
#define CH55X_DAP_SPI_H
#include "DAP.H"
#if DAP_SPI_SWD || DAP_SPI_JTAG
#ifndef DAP_SPI_DIV
#define DAP_SPI_DIV 6U
#endif
#if DAP_SPI_DIV < 6 || DAP_SPI_DIV > 255
#error Initial DAP SPI divider must be 6..255
#endif
extern bit DAP_SPI_Failed;
extern void DAP_SPI_Init(void);
#if DAP_SPI_JTAG
extern UINT8 DAP_SPI_Exchange(UINT8 value) small;
#if DAP_JTAG_ASM_ACTIVE
/* Internal DR entry: SPI already owns pins, FREE/RX empty, Failed is clear.
 * Caller checks Failed after every byte. Same R7 ABI/clobbers as Exchange.
 */
extern UINT8 DAP_SPI_ExchangeActive(UINT8 value) small;
#endif
#endif
#if DAP_SPI_SWD
extern void DAP_SPI_Write(UINT8 value);
extern UINT8 DAP_SPI_WriteWord(const UINT8 idata *datas);
#endif
#endif
#endif
