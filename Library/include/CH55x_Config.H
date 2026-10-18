#ifndef CH55X_CONFIG_H
#define CH55X_CONFIG_H

/* Shared clock source for the chip library and board/protocol configuration.
 * The current USB/UART/SWJ arithmetic build supports 24 MHz only. */
#ifndef SYSTEM_CLOCK_HZ
#define SYSTEM_CLOCK_HZ 24000000UL
#endif
#if SYSTEM_CLOCK_HZ != 24000000UL
#error This firmware requires a 24 MHz system clock
#endif

#endif
