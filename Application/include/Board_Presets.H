#ifndef CH55X_BOARD_PRESETS_H
#define CH55X_BOARD_PRESETS_H

// 0 保留原工程配置；1..7 提供后端能力组合和默认引脚。
// 预设不是运行时切换，也不代表该组合已完成本板硬件验收。
#define BOARD_IO_CUSTOM       0
#define BOARD_IO_SWD_JTAG     1
#define BOARD_IO_JTAG_RESET   2
#define BOARD_IO_SWD_SWO      3
#define BOARD_IO_SWD_SWO_SPI  4
#define BOARD_IO_JTAG_SPI     5
#define BOARD_IO_GPIO_SWJ     6
#define BOARD_IO_SWD_SPI_2WIRE 7
#ifndef BOARD_IO_PRESET
#define BOARD_IO_PRESET BOARD_IO_CUSTOM
#endif
#if BOARD_IO_PRESET < 0 || BOARD_IO_PRESET > 7
#error Unknown BOARD_IO_PRESET
#endif

#if BOARD_IO_PRESET != BOARD_IO_CUSTOM
#if BOARD_IO_PRESET == BOARD_IO_SWD_JTAG || BOARD_IO_PRESET == BOARD_IO_JTAG_RESET || BOARD_IO_PRESET == BOARD_IO_JTAG_SPI || BOARD_IO_PRESET == BOARD_IO_GPIO_SWJ
#define BOARD_PRESET_JTAG 1
#define BOARD_PRESET_SWO 0
#else
#define BOARD_PRESET_JTAG 0
#define BOARD_PRESET_SWO 1
#endif
#if BOARD_IO_PRESET == BOARD_IO_SWD_SWO_SPI || BOARD_IO_PRESET == BOARD_IO_SWD_SPI_2WIRE
#define BOARD_PRESET_SPI 1
#else
#define BOARD_PRESET_SPI 0
#endif
#if BOARD_IO_PRESET == BOARD_IO_JTAG_RESET || BOARD_IO_PRESET == BOARD_IO_JTAG_SPI || BOARD_IO_PRESET == BOARD_IO_GPIO_SWJ
#define BOARD_PRESET_TRST 1
#ifndef PIN_nTRST_BIT
#define PIN_nTRST_BIT 2
#endif
#else
#define BOARD_PRESET_TRST 0
#endif

// 不静默覆盖工程已有能力开关，冲突配置由编译指出。
#if defined(DAP_JTAG) && DAP_JTAG != BOARD_PRESET_JTAG
#error DAP_JTAG conflicts with BOARD_IO_PRESET
#endif
#ifndef DAP_JTAG
#define DAP_JTAG BOARD_PRESET_JTAG
#endif
#if defined(SWO_UART) && SWO_UART != BOARD_PRESET_SWO
#error SWO_UART conflicts with BOARD_IO_PRESET
#endif
#ifndef SWO_UART
#define SWO_UART BOARD_PRESET_SWO
#endif
#if defined(DAP_SPI_SWD) && DAP_SPI_SWD != BOARD_PRESET_SPI
#error DAP_SPI_SWD conflicts with BOARD_IO_PRESET
#endif
#ifndef DAP_SPI_SWD
#define DAP_SPI_SWD BOARD_PRESET_SPI
#endif
#if BOARD_IO_PRESET == BOARD_IO_JTAG_SPI
#if defined(DAP_SPI_JTAG) && !DAP_SPI_JTAG
#error DAP_SPI_JTAG conflicts with BOARD_IO_PRESET
#endif
#ifndef DAP_SPI_JTAG
#define DAP_SPI_JTAG 1
#endif
#endif

#if BOARD_IO_PRESET == BOARD_IO_GPIO_SWJ
// 与预设5共用接线，但两个调试协议均由GPIO实现。
#if defined(DAP_SPI_JTAG) && DAP_SPI_JTAG
#error GPIO SWJ preset must not enable JTAG SPI
#endif
#ifndef DAP_SPI_JTAG
#define DAP_SPI_JTAG 0
#endif
#endif

#if BOARD_IO_PRESET == BOARD_IO_JTAG_SPI || BOARD_IO_PRESET == BOARD_IO_GPIO_SWJ
#ifndef PIN_SWCLK_BIT
#define PIN_SWCLK_BIT 7
#endif
#ifndef PIN_SWDIO_PORT
#define PIN_SWDIO_PORT 3
#endif
#ifndef PIN_SWDIO_BIT
#define PIN_SWDIO_BIT 3
#endif
#ifndef PIN_TDI_BIT
#define PIN_TDI_BIT 5
#endif
#ifndef PIN_TDO_PORT
#define PIN_TDO_PORT 1
#endif
#ifndef PIN_TDO_BIT
#define PIN_TDO_BIT 6
#endif
#endif

#if BOARD_IO_PRESET == BOARD_IO_SWD_SPI_2WIRE
// 尽量复用预设5/6的引脚集合；双向数据移至硬件MISO P1.6。
#if defined(DAP_SPI_SWD_2WIRE) && !DAP_SPI_SWD_2WIRE
#error Two-wire preset requires DAP_SPI_SWD_2WIRE
#endif
#ifndef DAP_SPI_SWD_2WIRE
#define DAP_SPI_SWD_2WIRE 1
#endif
#ifndef PIN_SWCLK_BIT
#define PIN_SWCLK_BIT 7
#endif
#ifndef PIN_SWDIO_BIT
#define PIN_SWDIO_BIT 6
#endif
#ifndef DAP_SWD_ASM_WORD
#define DAP_SWD_ASM_WORD 1
#endif
#ifndef DAP_SWD_ASM_ACK
#define DAP_SWD_ASM_ACK 1
#endif
#endif
#if defined(DAP_HAS_NTRST) && DAP_HAS_NTRST != BOARD_PRESET_TRST
#error DAP_HAS_NTRST conflicts with BOARD_IO_PRESET
#endif
#ifndef DAP_HAS_NTRST
#define DAP_HAS_NTRST BOARD_PRESET_TRST
#endif

#if BOARD_IO_PRESET == BOARD_IO_SWD_SWO_SPI
// CH552 SPI0: SCK=P1.7，MOSI=P1.5；只能加速SWD写相位。
#ifndef PIN_SWCLK_BIT
#define PIN_SWCLK_BIT 7
#endif
#ifndef PIN_SWDIO_BIT
#define PIN_SWDIO_BIT 5
#endif
#ifndef DAP_SWD_ASM_WORD
#define DAP_SWD_ASM_WORD 1
#endif
#ifndef DAP_SWD_ASM_ACK
#define DAP_SWD_ASM_ACK 1
#endif
#endif
#endif
#endif
