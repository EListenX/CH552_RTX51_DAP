#ifndef CH55X_MATH_H
#define CH55X_MATH_H

// CH55x 专用接口，不替换 C51 内部库符号。高字节选择固定时钟，低 24 位为除数。
// 0: floor(12000000/divisor)；1/2: 最近实际频率的 Timer2 计数。
// 调用者先校验：mode0 divisor=1..6000000；mode1=23..1500000；mode2=6..22。
// mode1/2 等误差选择较小计数，返回值分别不超过 65218/62500。
// C51 单 long 入参/返回均为 R4..R7，无共享参数区；A51 内部只用寄存器和栈。
#define CH55X_CLOCK_SWJ 0x00000000UL
// mode3: max(1, ceil((ceil(12000000/divisor)-24)/14)), divisor=1..6000000.
// Experimental 24 MHz SWD delay-count mapping; caller saturates to 16 bits.
#define CH55X_CLOCK_SWJ_CALIBRATED 0x03000000UL
#define CH55X_CLOCK_UART_FAST 0x01000000UL
#define CH55X_CLOCK_UART_SLOW 0x02000000UL
extern UINT32 CH55x_ClockDivide(UINT32 operand);

#endif
