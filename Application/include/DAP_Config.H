/**
 * @file    DAP_Config.h
 * @brief   CMSIS-DAP target config for CH552/8051: SWJ-DP, SWD-DP, JTAG-DP
 * @note    Modify pin/clock for your hardware. Unimplemented items use defaults.
 */

#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#include "CH552.H"
#include "Board_Config.H"

/*----------------------------------*/
/* 1. Target hardware config        */
/*----------------------------------*/

// MCU clock (Hz)
#define CPU_CLOCK              SYSTEM_CLOCK_HZ // Shared chip/board clock

// IO port write cycles (8051 ~2 clock cycles)
#define IO_PORT_WRITE_CYCLES   (2U)

// 独立实验配置：24 MHz 慢档循环校准，默认保留已烧录路径。
#ifndef DAP_SWJ_CALIBRATED
#define DAP_SWJ_CALIBRATED 0
#endif
#ifndef DAP_CLOCK_ALIGNMENT_DIAG
#define DAP_CLOCK_ALIGNMENT_DIAG 0
#endif
#if DAP_CLOCK_ALIGNMENT_DIAG && !DAP_SWJ_CALIBRATED
#error Alignment experiment requires the calibrated profile
#endif

/*----------------------------------*/
/* 2. CMSIS‑DAP core configuration  */
/*----------------------------------*/

// Enable CMSIS‑DAP debug ports
// 1 = enabled, 0 = disabled
#define DAP_SWD                     1       // Enable SWD debug port
#ifndef DAP_SWD_ASM_DATA
#define DAP_SWD_ASM_DATA             1       // Shared fast/slow data-byte candidate.
#endif
#if DAP_SWD_ASM_DATA != 0 && DAP_SWD_ASM_DATA != 1
#error DAP_SWD_ASM_DATA must be 0 or 1
#endif
/* Diagnostic only: keep ASM transmit, use the existing C RDATA/parity loop. */
#ifndef DAP_SWD_DIAG_C_READ
#define DAP_SWD_DIAG_C_READ           0
#endif
#if DAP_SWD_DIAG_C_READ != 0 && DAP_SWD_DIAG_C_READ != 1
#error DAP_SWD_DIAG_C_READ must be 0 or 1
#endif
/* Complementary diagnostic: C request/data transmit, original ASM RDATA. */
#ifndef DAP_SWD_DIAG_C_WRITE
#define DAP_SWD_DIAG_C_WRITE          0
#endif
#if DAP_SWD_DIAG_C_WRITE != 0 && DAP_SWD_DIAG_C_WRITE != 1
#error DAP_SWD_DIAG_C_WRITE must be 0 or 1
#endif
#ifndef DAP_SWD_ASM_WORD
#define DAP_SWD_ASM_WORD             0       // Optional fast four-byte GPIO read candidate.
#endif
#if DAP_SWD_ASM_WORD != 0 && DAP_SWD_ASM_WORD != 1
#error DAP_SWD_ASM_WORD must be 0 or 1
#endif
#if DAP_SWD_ASM_WORD && !DAP_SWD_ASM_DATA
#error DAP_SWD_ASM_WORD requires the shared ASM byte backend
#endif
#ifndef DAP_SWD_ASM_ACK
#define DAP_SWD_ASM_ACK              0       // Fast initial turnaround + ACK candidate.
#endif
#if DAP_SWD_ASM_ACK != 0 && DAP_SWD_ASM_ACK != 1
#error DAP_SWD_ASM_ACK must be 0 or 1
#endif
#if DAP_SWD_ASM_ACK && !DAP_SWD_ASM_DATA
#error DAP_SWD_ASM_ACK requires the shared ASM byte backend
#endif
#ifndef DAP_JTAG
#define DAP_JTAG                    1       // GPIO JTAG; SWD remains the default port.
#endif
#ifndef DAP_SPI_SWD
#define DAP_SPI_SWD                 0       // SPI write + GPIO read, separate pin map.
#endif
#ifndef DAP_SPI_SWD_SWJ
#define DAP_SPI_SWD_SWJ             0       // Opt-in SWD SPI plus GPIO JTAG candidate.
#endif
#if DAP_SPI_SWD_SWJ != 0 && DAP_SPI_SWD_SWJ != 1
#error DAP_SPI_SWD_SWJ must be 0 or 1
#endif
// 两线半双工候选：SCK=P1.7，双向MISO=P1.6；读相位仍为GPIO。
#ifndef DAP_SPI_SWD_2WIRE
#define DAP_SPI_SWD_2WIRE           0
#endif
#if DAP_SPI_SWD_2WIRE != 0 && DAP_SPI_SWD_2WIRE != 1
#error DAP_SPI_SWD_2WIRE must be 0 or 1
#endif
#if DAP_SPI_SWD_2WIRE && !DAP_SPI_SWD
#error Two-wire SWD requires the SWD SPI backend
#endif
/* 请求头GPIO候选：保留SPI四字节写，避免请求头接管外设。 */
#ifndef DAP_SPI_SWD_GPIO_REQUEST
#define DAP_SPI_SWD_GPIO_REQUEST 0
#endif
#if DAP_SPI_SWD_GPIO_REQUEST != 0 && DAP_SPI_SWD_GPIO_REQUEST != 1
#error DAP_SPI_SWD_GPIO_REQUEST must be 0 or 1
#endif
#if DAP_SPI_SWD_GPIO_REQUEST && (!DAP_SPI_SWD || !DAP_SWD_ASM_DATA)
#error GPIO request candidate requires SPI writes and the ASM byte backend
#endif
/* SPI移位期间预取下一字节，等待完成后再写FIFO。 */
#ifndef DAP_SWD_FUSED_REQUEST
#define DAP_SWD_FUSED_REQUEST 0
#endif
#if DAP_SWD_FUSED_REQUEST != 0 && DAP_SWD_FUSED_REQUEST != 1
#error DAP_SWD_FUSED_REQUEST must be 0 or 1
#endif
#if DAP_SWD_FUSED_REQUEST && (!DAP_SWD_ASM_DATA || !DAP_SPI_SWD_GPIO_REQUEST)
#error Fused request requires the GPIO request ASM backend
#endif
#ifndef DAP_SWD_FUSED_READ
#define DAP_SWD_FUSED_READ 0
#endif
#if DAP_SWD_FUSED_READ != 0 && DAP_SWD_FUSED_READ != 1
#error DAP_SWD_FUSED_READ must be 0 or 1
#endif
#if DAP_SWD_FUSED_READ && !DAP_SWD_ASM_WORD
#error Fused read requires the four-byte ASM read backend
#endif
#ifndef DAP_SWD_BLOCK_DIRECT
#define DAP_SWD_BLOCK_DIRECT 0
#endif
#if DAP_SWD_BLOCK_DIRECT != 0 && DAP_SWD_BLOCK_DIRECT != 1
#error DAP_SWD_BLOCK_DIRECT must be 0 or 1
#endif
#if DAP_SWD_BLOCK_DIRECT && (!DAP_SWD || DAP_JTAG)
#error Direct SWD block candidate requires SWD without JTAG
#endif
#ifndef DAP_EP1_ATOMIC_CTRL
#define DAP_EP1_ATOMIC_CTRL 0
#endif
#if DAP_EP1_ATOMIC_CTRL != 0 && DAP_EP1_ATOMIC_CTRL != 1
#error DAP_EP1_ATOMIC_CTRL must be 0 or 1
#endif
#ifndef DAP_SWD_ACK_WRITE_ONLY
#define DAP_SWD_ACK_WRITE_ONLY 0
#endif
#if DAP_SWD_ACK_WRITE_ONLY != 0 && DAP_SWD_ACK_WRITE_ONLY != 1
#error DAP_SWD_ACK_WRITE_ONLY must be 0 or 1
#endif

#ifndef DAP_SWD_ACK_WRITE_TURN
#define DAP_SWD_ACK_WRITE_TURN 0
#endif
#if DAP_SWD_ACK_WRITE_TURN != 0 && DAP_SWD_ACK_WRITE_TURN != 1
#error DAP_SWD_ACK_WRITE_TURN must be 0 or 1
#endif
#if DAP_SWD_ACK_WRITE_TURN && !DAP_SWD_ASM_ACK
#error ACK write turnaround requires the ASM ACK backend
#endif
#if DAP_SWD_ACK_WRITE_ONLY && !DAP_SWD_ACK_WRITE_TURN
#error Separate write ACK requires ACK write turnaround
#endif

#ifndef DAP_SWD_FUSED_PARITY
#define DAP_SWD_FUSED_PARITY 0
#endif
#if DAP_SWD_FUSED_PARITY != 0 && DAP_SWD_FUSED_PARITY != 1
#error DAP_SWD_FUSED_PARITY must be 0 or 1
#endif
#if DAP_SWD_FUSED_PARITY && (!DAP_SPI_SWD || !DAP_SWD_ASM_DATA || DAP_SWD_DIAG_C_WRITE)
#error Fused parity requires the SWD SPI and ASM write backend
#endif

#ifndef DAP_SWD_SMALL_CORE
#define DAP_SWD_SMALL_CORE 0
#endif
#if DAP_SWD_SMALL_CORE != 0 && DAP_SWD_SMALL_CORE != 1
#error DAP_SWD_SMALL_CORE must be 0 or 1
#endif

#ifndef DAP_EP1_MANUAL_TOG
#define DAP_EP1_MANUAL_TOG 0
#endif
#if DAP_EP1_MANUAL_TOG != 0 && DAP_EP1_MANUAL_TOG != 1
#error DAP_EP1_MANUAL_TOG must be 0 or 1
#endif
#ifndef DAP_SPI_SWD_PREFETCH
#define DAP_SPI_SWD_PREFETCH 0
#endif
#if DAP_SPI_SWD_PREFETCH != 0 && DAP_SPI_SWD_PREFETCH != 1
#error DAP_SPI_SWD_PREFETCH must be 0 or 1
#endif
#if DAP_SPI_SWD_PREFETCH && !DAP_SPI_SWD
#error SPI prefetch requires the SWD SPI backend
#endif
#ifndef DAP_SPI_JTAG
#define DAP_SPI_JTAG                0       // Full-duplex bytes; TAP transitions remain GPIO.
#endif
#if DAP_SPI_JTAG != 0 && DAP_SPI_JTAG != 1
#error DAP_SPI_JTAG must be 0 or 1
#endif
#if DAP_SPI_JTAG && (!DAP_JTAG || DAP_SPI_SWD)
#error JTAG SPI requires JTAG and exclusive SPI ownership
#endif
#ifndef DAP_SPI_JTAG_STREAM
#define DAP_SPI_JTAG_STREAM         0       // Keep SPI pins between constant-TMS bytes.
#endif
#if DAP_SPI_JTAG_STREAM != 0 && DAP_SPI_JTAG_STREAM != 1
#error DAP_SPI_JTAG_STREAM must be 0 or 1
#endif
#if DAP_SPI_JTAG_STREAM && !DAP_SPI_JTAG
#error JTAG SPI streaming requires the JTAG SPI backend
#endif
#ifndef DAP_SPI_JTAG_WORD
#define DAP_SPI_JTAG_WORD           0       // Fuse DR byte loop and FIFO handling.
#endif
#if DAP_SPI_JTAG_WORD != 0 && DAP_SPI_JTAG_WORD != 1
#error DAP_SPI_JTAG_WORD must be 0 or 1
#endif
#if DAP_SPI_JTAG_WORD && !DAP_SPI_JTAG_STREAM
#error JTAG word batching requires streaming pin ownership
#endif
#ifndef DAP_JTAG_GPIO_CORE
#define DAP_JTAG_GPIO_CORE          0       // Reuse GPIO hot-path optimizations without SPI.
#endif
#if DAP_JTAG_GPIO_CORE != 0 && DAP_JTAG_GPIO_CORE != 1
#error DAP_JTAG_GPIO_CORE must be 0 or 1
#endif
#ifndef DAP_JTAG_GPIO_BYTE
#define DAP_JTAG_GPIO_BYTE          0       // Shared fast GPIO byte loop for Sequence and DR.
#endif
#if DAP_JTAG_GPIO_BYTE != 0 && DAP_JTAG_GPIO_BYTE != 1
#error DAP_JTAG_GPIO_BYTE must be 0 or 1
#endif
#if DAP_JTAG_GPIO_BYTE && !DAP_JTAG_GPIO_CORE
#error GPIO JTAG byte loop requires the GPIO core backend
#endif
#ifndef DAP_JTAG_GPIO_TIGHT
#define DAP_JTAG_GPIO_TIGHT         0       // Isolated GPIO code-generation/loop candidate.
#endif
#if DAP_JTAG_GPIO_TIGHT != 0 && DAP_JTAG_GPIO_TIGHT != 1
#error DAP_JTAG_GPIO_TIGHT must be 0 or 1
#endif
#if DAP_JTAG_GPIO_TIGHT && !DAP_JTAG_GPIO_BYTE
#error Tight GPIO JTAG requires the shared GPIO byte backend
#endif

#ifndef DAP_JTAG_COMPACT_DISPATCH
#define DAP_JTAG_COMPACT_DISPATCH   0       // Isolated DAP dispatcher code-generation candidate.
#endif
#if DAP_JTAG_COMPACT_DISPATCH != 0 && DAP_JTAG_COMPACT_DISPATCH != 1
#error DAP_JTAG_COMPACT_DISPATCH must be 0 or 1
#endif
#if DAP_JTAG_COMPACT_DISPATCH && !DAP_SPI_JTAG && !DAP_JTAG_GPIO_CORE
#error Compact JTAG dispatcher requires SPI JTAG or the GPIO JTAG core
#endif

#ifndef DAP_JTAG_FAST_CACHE
#define DAP_JTAG_FAST_CACHE         0       // Snapshot XDATA clock mode per command.
#endif
#if DAP_JTAG_FAST_CACHE != 0 && DAP_JTAG_FAST_CACHE != 1
#error DAP_JTAG_FAST_CACHE must be 0 or 1
#endif
#if DAP_JTAG_FAST_CACHE && !DAP_SPI_JTAG_WORD && !DAP_JTAG_GPIO_CORE
#error JTAG clock cache requires the fused JTAG word backend or GPIO JTAG core
#endif
#ifndef DAP_JTAG_TAP_PATH
#define DAP_JTAG_TAP_PATH           0       // Batch fixed TAP navigation, not data/ACK.
#endif
#if DAP_JTAG_TAP_PATH != 0 && DAP_JTAG_TAP_PATH != 1
#error DAP_JTAG_TAP_PATH must be 0 or 1
#endif
#ifndef DAP_JTAG_ASM_FIFO
#define DAP_JTAG_ASM_FIFO           0       // Shared current-bank SPI FIFO byte leaf.
#endif
#if DAP_JTAG_ASM_FIFO != 0 && DAP_JTAG_ASM_FIFO != 1
#error DAP_JTAG_ASM_FIFO must be 0 or 1
#endif
#if DAP_JTAG_ASM_FIFO && !DAP_JTAG_FAST_CACHE
#error JTAG ASM FIFO requires the cached fused SPI backend
#endif
#ifndef DAP_JTAG_ASM_ACTIVE
#define DAP_JTAG_ASM_ACTIVE         0       // DR caller owns SPI setup and failure guard.
#endif
#if DAP_JTAG_ASM_ACTIVE != 0 && DAP_JTAG_ASM_ACTIVE != 1
#error DAP_JTAG_ASM_ACTIVE must be 0 or 1
#endif
#if DAP_JTAG_ASM_ACTIVE && !DAP_JTAG_ASM_FIFO
#error JTAG active entry requires the shared ASM FIFO backend
#endif
#if DAP_JTAG_TAP_PATH && (!DAP_JTAG_FAST_CACHE || DAP_JTAG_ASM_FIFO)
#error JTAG TAP path requires cached C FIFO baseline
#endif
#ifndef DAP_JTAG_IR_CACHE
#define DAP_JTAG_IR_CACHE           0       // Snapshot chain count/index per IR scan.
#endif
#if DAP_JTAG_IR_CACHE != 0 && DAP_JTAG_IR_CACHE != 1
#error DAP_JTAG_IR_CACHE must be 0 or 1
#endif
#if DAP_JTAG_IR_CACHE && !DAP_JTAG_TAP_PATH
#error JTAG IR cache requires the TAP path baseline
#endif
#ifndef DAP_JTAG_BIT_CLOCK
#define DAP_JTAG_BIT_CLOCK          0       // Return GPIO TDO in the C51 carry bit.
#endif
#if DAP_JTAG_BIT_CLOCK != 0 && DAP_JTAG_BIT_CLOCK != 1
#error DAP_JTAG_BIT_CLOCK must be 0 or 1
#endif
#if DAP_JTAG_BIT_CLOCK && (!DAP_JTAG_TAP_PATH || DAP_JTAG_IR_CACHE)
#error JTAG bit clock requires the TAP path baseline without IR snapshot
#endif
#if DAP_JTAG_GPIO_CORE && (!DAP_JTAG || DAP_SPI_JTAG || !DAP_JTAG_FAST_CACHE || !DAP_JTAG_TAP_PATH || !DAP_JTAG_BIT_CLOCK)
#error GPIO JTAG core requires GPIO JTAG with clock cache TAP path and bit return
#endif
#if DAP_JTAG != 0 && DAP_JTAG != 1
#error DAP_JTAG must be 0 or 1
#endif
#if DAP_SPI_SWD != 0 && DAP_SPI_SWD != 1
#error DAP_SPI_SWD must be 0 or 1
#endif
#define DAP_JTAG_DEV_CNT            8       // Maximum number of JTAG devices in chain
// 可选 nRESET 接线：默认未确认物理接线，Keil 使用 SYSRESETREQ 软件复位。
#ifndef DAP_HAS_NRESET
#define DAP_HAS_NRESET               0
#endif
// 目前支持即时 ExecuteCommands，但没有 QueueCommands 队列，不声明 Atomic 能力。
#define DAP_ATOMIC_COMMANDS          0
#if (DAP_ATOMIC_COMMANDS != 0)
#error This build has no atomic queue implementation
#endif
#ifndef DAP_HAS_NTRST
#define DAP_HAS_NTRST               0       // Optional wire, not assumed connected.
#endif

/* Temporary read-only GPIO diagnostics; enabled explicitly by the Keil build. */
#ifndef DAP_GPIO_DIAGNOSTICS
#define DAP_GPIO_DIAGNOSTICS         0
#endif
#define DAP_GPIO_DIAG_COMMAND        0x82U
#define DAP_GPIO_DIAG_RESPONSE_SIZE  14U

/* Read-only RTX timer checkpoint diagnostic, disabled in normal builds. */
#ifndef RTX_STACK_DIAG
#define RTX_STACK_DIAG              0
#endif
#if RTX_STACK_DIAG != 0 && RTX_STACK_DIAG != 1
#error RTX_STACK_DIAG must be 0 or 1
#endif
#define RTX_STACK_DIAG_COMMAND      0x83U
#define RTX_STACK_DIAG_RESPONSE_SIZE 3U

// Default debug port / clock
#define DAP_DEFAULT_PORT            1       // Default port: 1 = SWD, 2 = JTAG
#define DAP_DEFAULT_SWJ_CLOCK       1000000 // Default SWJ clock in Hz

// DAP packet layout (must match USB endpoints)
#define DAP_PACKET_SIZE             64      // Single command/response packet size in bytes
#ifndef DAP_PACKET_COUNT
#define DAP_PACKET_COUNT            1       // Number of buffered packets
#endif
#if DAP_PACKET_COUNT != 1 && DAP_PACKET_COUNT != 2
#error DAP packet count must fit the two EP1 OUT buffers
#endif

/*----------------------------------*/
/* 5. SWO / trace (optional)        */
/*----------------------------------*/

// SWO trace configuration: 0 = disabled, 1 = enabled
#ifndef SWO_UART
#define SWO_UART               (0U)
#endif
#define SWO_UART_MAX_BAUDRATE  (1500000UL)
#define SWO_MANCHESTER         (0U)
#define SWO_BUFFER_SIZE        (128U)
#define SWO_STREAM             (0U)
#if SWO_UART != 0 && SWO_UART != 1
#error SWO_UART must be 0 or 1
#endif
#ifndef DAP_GPIO_SWJ_SWO
#define DAP_GPIO_SWJ_SWO 0
#endif
#if DAP_GPIO_SWJ_SWO != 0 && DAP_GPIO_SWJ_SWO != 1
#error DAP_GPIO_SWJ_SWO must be 0 or 1
#endif
#if DAP_GPIO_SWJ_SWO && (!DAP_JTAG || !SWO_UART || !DAP_JTAG_GPIO_CORE || DAP_SPI_SWD || DAP_SPI_JTAG)
#error GPIO SWJ SWO requires GPIO JTAG and SWO without SPI
#endif
#if DAP_GPIO_SWJ_SWO
#define DAP_SWO_MEMORY small
#else
#define DAP_SWO_MEMORY
#endif
#if SWO_UART && DAP_JTAG && !DAP_GPIO_SWJ_SWO
#error SWO profile requires DAP_JTAG=0 to fit CH552 ROM
#endif
#if DAP_SPI_SWD && (!DAP_SWD_ASM_DATA || (DAP_JTAG && !DAP_SPI_SWD_SWJ))
#error SWD SPI profile requires ASM data and DAP_JTAG=0
#endif
#if DAP_SPI_SWD_SWJ && (!DAP_SPI_SWD || !DAP_SPI_SWD_2WIRE || !DAP_JTAG || !DAP_JTAG_GPIO_CORE || SWO_UART)
#error SWD SPI SWJ requires two-wire SPI, GPIO JTAG core and no SWO
#endif
#if DAP_SWJ_CALIBRATED != 0 && DAP_SWJ_CALIBRATED != 1
#error DAP_SWJ_CALIBRATED must be 0 or 1
#endif
#if DAP_SWJ_CALIBRATED && (!DAP_SWD_ASM_DATA || !DAP_SPI_SWD || DAP_JTAG)
#error Clock calibration candidate requires the SWD SPI ASM profile
#endif
#if DAP_SWD_DIAG_C_READ && (!DAP_SWD_ASM_DATA || DAP_SWD_ASM_WORD || DAP_SWD_ASM_ACK || DAP_SPI_SWD || DAP_JTAG)
#error C read diagnostic requires ASM bytes only, without SPI word ACK or JTAG
#endif
#if DAP_SWD_DIAG_C_WRITE && (!DAP_SWD_ASM_DATA || DAP_SWD_ASM_WORD || DAP_SWD_ASM_ACK || DAP_SPI_SWD || DAP_JTAG || DAP_SWD_DIAG_C_READ)
#error C write diagnostic requires ASM bytes only without other diagnostic or accelerators
#endif
#if DAP_HAS_NTRST && !DAP_JTAG
#error nTRST requires DAP_JTAG=1; SWD can remain enabled
#endif

/*----------------------------------*/
/* 9. Protocol mode / interfaces    */
/*----------------------------------*/

// DAP DP interfaces supported by this build
#define DAP_INTERFACE_SWD       (1U)
#define DAP_INTERFACE_JTAG      (2U)
#define DAP_INTERFACE_SWJ       (3U)

/*
 * Protocol mode selection (used before DAP_Connect):
 *   1. SWD   – SWD‑only operation
 *   2. JTAG  – full JTAG (TCK/TMS/TDI/TDO/nTRST/nRESET)
 *   3. SWJ   – SWD and JTAG; host can switch at run‑time
 */

/*----------------------------------*/
/* 10. Optional DAP features        */
/*----------------------------------*/

/*
 * Enable support for DAP_Transfer with match‑mask feature.
 * 0 = disabled (saves external RAM)
 * 1 = enabled  (fully compliant with CMSIS‑DAP transfer‑match command)
 */
#define ENABLE_DAP_MATCH  (1U)

/**
 * @brief   Use fixed target‑device identification.
 *
 * 1 = DAP_Info returns the fixed target device strings below,
 *     regardless of the actually attached target.
 * 0 = DAP_Info is expected to reflect the real target device
 *     (via probe, host configuration, or higher‑level firmware).
 */
#define TARGET_DEVICE_FIXED   (0U)

/**
 * @brief  Target device vendor name.
 */
#define TARGET_DEVICE_VENDOR  "WhyCan"

/**
 * @brief  Target device product name / model.
 */
#define TARGET_DEVICE_PRODUCT "CH552 CMSIS-DAP"

/**
 * @brief  Firmware / hardware version string.
 */
#if RTX_STACK_DIAG
/* Short unique diagnostic identity keeps the isolated image below ISP. */
#define TARGET_DEVICE_FW_VER  "stk1"
#elif DAP_SPI_SWD_SWJ
#define TARGET_DEVICE_FW_VER  "ssj1"
#elif DAP_GPIO_SWJ_SWO
#define TARGET_DEVICE_FW_VER  "gsf1"
#elif DAP_JTAG_GPIO_TIGHT
#define TARGET_DEVICE_FW_VER  "gjt1"
#elif DAP_JTAG_COMPACT_DISPATCH && DAP_JTAG_GPIO_CORE
#define TARGET_DEVICE_FW_VER  "gcp1"
#elif defined(DAP_GPIO_SWO_PROFILE)
#if DAP_GPIO_SWO_PROFILE != 1 || DAP_JTAG || DAP_SPI_SWD || !SWO_UART
#error GPIO SWO profile requires GPIO SWD and UART SWO without JTAG
#endif
#define TARGET_DEVICE_FW_VER  "gso1"
#elif defined(BOARD_IO_GPIO_SWJ) && BOARD_IO_PRESET == BOARD_IO_GPIO_SWJ
#if DAP_JTAG_GPIO_CORE
#if DAP_JTAG_GPIO_BYTE
#if defined(CDC_EP2_MANUAL_TOG) && CDC_EP2_MANUAL_TOG
#if defined(CDC_UART_PRIORITY_FIX) && CDC_UART_PRIORITY_FIX
#if defined(CDC_STATE_MASK_FAST) && CDC_STATE_MASK_FAST && defined(CDC_RX_BATCH) && CDC_RX_BATCH
#if defined(CDC_RX_FIFO) && CDC_RX_FIFO
#if defined(CDC_RX_RING) && CDC_RX_RING
#if defined(CDC_EP2_XDATA) && CDC_EP2_XDATA
#if defined(CDC_RX_SNAPSHOT_COMPACT) && CDC_RX_SNAPSHOT_COMPACT
#if defined(CDC_RX_SNAPSHOT_SERVICE) && CDC_RX_SNAPSHOT_SERVICE
#if defined(CDC_RX_BATCH_MS) && CDC_RX_BATCH_MS == 2
#if defined(CDC_RX_RING_SIZE) && CDC_RX_RING_SIZE == 128
#if defined(CDC_RX_IDLE_ZLP) && CDC_RX_IDLE_ZLP
#define TARGET_DEVICE_FW_VER  "gsjh"
#else
#define TARGET_DEVICE_FW_VER  "gsjf"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsje"
#endif
#else
#if defined(CDC_RX_RING_SIZE) && CDC_RX_RING_SIZE == 128
#if defined(CDC_RX_IDLE_ZLP) && CDC_RX_IDLE_ZLP
#define TARGET_DEVICE_FW_VER  "gsji"
#else
#define TARGET_DEVICE_FW_VER  "gsjg"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsjd"
#endif
#endif
#elif defined(CDC_RX_SNAPSHOT_IRQ) && CDC_RX_SNAPSHOT_IRQ
#define TARGET_DEVICE_FW_VER  "gsjc"
#else
#define TARGET_DEVICE_FW_VER  "gsja"
#endif
#elif defined(CDC_DAP_YIELD) && CDC_DAP_YIELD
#define TARGET_DEVICE_FW_VER  "gsjb"
#else
#define TARGET_DEVICE_FW_VER  "gsj9"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj8"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj7"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj6"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj5"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj4"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj3"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj2"
#endif
#else
#define TARGET_DEVICE_FW_VER  "gsj1"
#endif
#elif DAP_SPI_JTAG
#if DAP_JTAG_COMPACT_DISPATCH
#if DAP_HAS_NRESET
#define TARGET_DEVICE_FW_VER  "jsr1"
#else
#define TARGET_DEVICE_FW_VER  "jcd1"
#endif
#elif DAP_JTAG_BIT_CLOCK
#define TARGET_DEVICE_FW_VER  "jbc1"
#elif DAP_JTAG_IR_CACHE
#define TARGET_DEVICE_FW_VER  "jir1"
#elif DAP_JTAG_TAP_PATH
#define TARGET_DEVICE_FW_VER  "jtp1"
#elif DAP_JTAG_ASM_ACTIVE
#define TARGET_DEVICE_FW_VER  "jaa1"
#elif DAP_JTAG_ASM_FIFO
#define TARGET_DEVICE_FW_VER  "jaf1"
#elif DAP_JTAG_FAST_CACHE
#define TARGET_DEVICE_FW_VER  "jpc1"
#elif DAP_SPI_JTAG_WORD
#define TARGET_DEVICE_FW_VER  "jpw1"
#elif DAP_SPI_JTAG_STREAM
#if defined(DAP_SPI_DIV) && DAP_SPI_DIV == 12
#define TARGET_DEVICE_FW_VER  "jsp3"
#else
#define TARGET_DEVICE_FW_VER  "jsp4"
#endif
#elif defined(DAP_SPI_DIV) && DAP_SPI_DIV == 12
#define TARGET_DEVICE_FW_VER  "jsp2"
#else
#define TARGET_DEVICE_FW_VER  "jsp1"
#endif
#elif defined(DAP_JTAG_SPI_BENCH_GPIO) && DAP_JTAG_SPI_BENCH_GPIO
#if !DAP_JTAG || DAP_SPI_SWD || SWO_UART
#error JTAG SPI GPIO comparison requires GPIO JTAG without SWO
#endif
#define TARGET_DEVICE_FW_VER  "jpg1"
#elif defined(DAP_SPI_BENCH_GPIO) && DAP_SPI_BENCH_GPIO
#if DAP_SPI_SWD || DAP_JTAG || !SWO_UART
#error GPIO SPI comparison requires SWO enabled, JTAG and SPI disabled
#endif
#if defined(DAP_SPI_2WIRE_BENCH_GPIO) && DAP_SPI_2WIRE_BENCH_GPIO
#if DAP_PACKET_COUNT == 2
#define TARGET_DEVICE_FW_VER  "CH552-20261013-twg1"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261013-twg2"
#endif
#elif DAP_SWD_DIAG_C_WRITE
#define TARGET_DEVICE_FW_VER  "CH552-20261009-scw1"
#elif DAP_SWD_DIAG_C_READ
#define TARGET_DEVICE_FW_VER  "CH552-20261009-scr1"
#elif DAP_SWD_ASM_DATA
#define TARGET_DEVICE_FW_VER  "CH552-20261009-spg2"
#else
/* Distinguish the all-C SWD GPIO diagnostic from the ASM byte baseline. */
#define TARGET_DEVICE_FW_VER  "CH552-20261009-spc1"
#endif
#elif DAP_SPI_SWD
#if DAP_SPI_SWD_2WIRE
#if DAP_PACKET_COUNT == 2
#if DAP_SPI_SWD_GPIO_REQUEST && DAP_SPI_SWD_PREFETCH
#if DAP_SWD_FUSED_REQUEST
#if DAP_SWD_FUSED_READ
#if DAP_SWD_BLOCK_DIRECT
#if DAP_EP1_ATOMIC_CTRL
#if DAP_EP1_MANUAL_TOG
#if DAP_SWD_SMALL_CORE
#if DAP_SWD_FUSED_PARITY
#if DAP_SWD_ACK_WRITE_TURN
#if DAP_SWD_ACK_WRITE_ONLY
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tat2"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tat1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tpt1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tsc1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tbd3"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tbd2"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tbd1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tfr1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tpf1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-tpq1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261013-tws1"
#endif
#elif DAP_SPI_SWD_GPIO_REQUEST
#if DAP_SPI_SWD_PREFETCH
#define TARGET_DEVICE_FW_VER  "CH552-20261014-twp1"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261014-twh1"
#endif
#elif defined(DAP_SPI_DIV) && DAP_SPI_DIV == 12
/* 同引脚单包2MHz候选：仅降低SPI写相位速率，GPIO相位不变。 */
#define TARGET_DEVICE_FW_VER  "CH552-20261014-twm1"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261013-tws2"
#endif
#elif DAP_SWJ_CALIBRATED
#if DAP_CLOCK_ALIGNMENT_DIAG
#if defined(CDC_IN_HANDOFF_DIAG) && CDC_IN_HANDOFF_DIAG
#if defined(CDC_IN_GPIO_DIAG) && CDC_IN_GPIO_DIAG
#define TARGET_DEVICE_FW_VER  "CH552-20261010-cgp1"
#elif defined(CDC_RX_BATCH) && CDC_RX_BATCH
#define TARGET_DEVICE_FW_VER  "CH552-20261010-cbp1"
#elif defined(CDC_RX_SNAPSHOT_DIAG) && CDC_RX_SNAPSHOT_DIAG
#define TARGET_DEVICE_FW_VER  "CH552-20261010-crx1"
#elif defined(CDC_EP2_MANUAL_TOG) && CDC_EP2_MANUAL_TOG
#define TARGET_DEVICE_FW_VER  "CH552-20261010-cmt1"
#elif defined(CDC_IN_SNAPSHOT_DIAG) && CDC_IN_SNAPSHOT_DIAG
#define TARGET_DEVICE_FW_VER  "CH552-20261010-cis1"
#elif defined(CDC_RTX_NO_RR_DIAG) && CDC_RTX_NO_RR_DIAG
#define TARGET_DEVICE_FW_VER  "CH552-20261010-crr0"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261010-cin1"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261009-clk2"
#endif
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261009-clk1"
#endif
#elif DAP_PACKET_COUNT == 2
#define TARGET_DEVICE_FW_VER  "CH552-20261012-spq1"
#elif DAP_SWD_ASM_ACK
#define TARGET_DEVICE_FW_VER  "CH552-20261009-spw5"
#elif DAP_SWD_ASM_WORD
#define TARGET_DEVICE_FW_VER  "CH552-20261009-spw4"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261009-spw3"
#endif
#elif SWO_UART
#define TARGET_DEVICE_FW_VER  "CH552-20261009-swo1"
#elif DAP_HAS_NTRST
#define TARGET_DEVICE_FW_VER  "CH552-20261009-trs1"
#elif DAP_SWD_ASM_DATA
#define TARGET_DEVICE_FW_VER  "CH552-20261009-sba2"
#else
#define TARGET_DEVICE_FW_VER  "CH552-20261008-jtg5"
#endif
#define DAP_PROTOCOL_VERSION  "2.0.0"

/* Aliases for DAP.C DAP_Info */
#define DAP_FW_VER           DAP_PROTOCOL_VERSION
#define TargetDeviceVendor   TARGET_DEVICE_VENDOR
#define TargetDeviceName     TARGET_DEVICE_PRODUCT

/**
 * @brief  Timestamp clock frequency in Hz.
 *
 * This value is returned by DAP_Info(DAP_ID_TIMESTAMP_CLOCK) if
 * timestamp support is implemented.  Set to the MCU system clock
 * (for CH552 typically 24 MHz) or 0U to indicate “not supported”.
 */
#define TIMESTAMP_CLOCK   (0U) // 没有真实测试域计时器时不得声明时间戳能力。

#endif /* __DAP_CONFIG_H__ */
