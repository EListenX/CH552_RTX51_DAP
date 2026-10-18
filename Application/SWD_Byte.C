/* Shared fast/slow data-byte backend; C fallback is in SW_DP.c.
 * C51 SRC emits board pin equates; A51 uses current-bank registers only.
 * Delay is zero for fast mode, otherwise the existing 16-bit loop count.
 * Clobbers A/P, B, CY, DPTR, R2..R7; no static state or interrupt masking.
 * Delay leaf preserves the shifting accumulator, not caller B.
 * C51 REGPARMS: delay R6:R7, write value R5; byte input/result R7.
 * NOAREGS prevents compiler prologues using absolute bank-zero ARn operands.
 * Maximum extra return stack: 4 B (C caller + internal delay call).
 * The delay count matches SAFE_MOD increments, not the original clock period.
 */
#include "Board_Config.H"
#include "DAP_Config.H"
#pragma NOAREGS

#if DAP_JTAG_ASM_FIFO
/* Shared JTAG SPI byte leaf, used by Sequence and the DR loop.
 * REGPARMS: byte/result R7. Clobbers A/P, CY, R6/R7 only.
 * Preserve R0..R5/B/DPTR, current bank, no RAM scratch/IRQ masking/calls.
 * TMS and GPIO exits remain board-owned. RX is consumed once after FREE.
 */
extern bit DAP_SPI_Failed;
unsigned char DAP_SPI_Exchange(unsigned char value) small
{
    if (DAP_SPI_Failed) { return 0U; }
    ACC = value;
#pragma asm
    MOV SPI0_CTRL,#068H
#if DAP_JTAG_ASM_ACTIVE
    ; DR calls this label directly with R7; no second body or return stack.
    ; Only the guarded DR loop may bypass Failed and CTRL setup above.
    PUBLIC _DAP_SPI_ExchangeActive
_DAP_SPI_ExchangeActive:
    MOV A,R7
#endif
    MOV SPI0_DATA,A
    MOV R6,#0
jtag_spi_byte_wait:
    JB S0_FREE,jtag_spi_byte_ready
    DJNZ R6,jtag_spi_byte_wait
    SETB DAP_SPI_Failed
jtag_spi_byte_ready:
    MOV A,R7
    RLC A
    MOV PIN_TDI,C
    JB DAP_SPI_Failed,jtag_spi_byte_abort
    MOV A,SPI0_DATA
    SJMP jtag_spi_byte_done
jtag_spi_byte_abort:
    MOV SPI0_CTRL,#00AH
    MOV SPI0_CTRL,#008H
    CLR A
jtag_spi_byte_done:
#pragma endasm
    return ACC;
}
#endif

#if DAP_SWD_ASM_DATA
#ifdef FREQ_SYS
#error Shared ASM delay requires the current no-FREQ_SYS SW_DP timing profile
#endif

void SWD_SharedWrite(unsigned short delay, unsigned char value)
{
    DPL = (unsigned char)delay;
    DPH = (unsigned char)(delay >> 8);
    ACC = value;
#pragma asm
    MOV R4,DPH
    MOV R5,DPL
    MOV R6,#8
    MOV R3,A            ; Only live until the delay branch has restored A.
    MOV A,R4
    ORL A,R5
    JZ shared_write_fast
    MOV A,R3
shared_write_loop:
    RRC A
    MOV PIN_SWDIO,C
    CLR PIN_SWCLK
    LCALL SWD_SharedDelayBody
    SETB PIN_SWCLK
    LCALL SWD_SharedDelayBody
    DJNZ R6,shared_write_loop
    SJMP shared_write_done
shared_write_fast:
    MOV A,R3
shared_write_fast_loop:
    RRC A
    MOV PIN_SWDIO,C
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_write_fast_loop
shared_write_done:
#pragma endasm
}

unsigned char SWD_SharedRead(unsigned short delay)
{
    DPL = (unsigned char)delay;
    DPH = (unsigned char)(delay >> 8);
#pragma asm
    MOV R4,DPH
    MOV R5,DPL
    MOV R6,#8
    MOV A,R4
    ORL A,R5
    JZ shared_read_fast
    ; Eight sampled carry bits replace every original ACC bit; no CLR A needed.
shared_read_loop:
    CLR PIN_SWCLK
    LCALL SWD_SharedDelayBody
    MOV C,PIN_SWDIO
    RRC A
    SETB PIN_SWCLK
    LCALL SWD_SharedDelayBody
    DJNZ R6,shared_read_loop
    SJMP shared_read_done
shared_read_fast:
shared_read_fast_loop:
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    RRC A
    SETB PIN_SWCLK
    DJNZ R6,shared_read_fast_loop
shared_read_done:
#pragma endasm
    return ACC;
}

unsigned char SWD_SharedParity(unsigned char value)
{
    ACC = value;
#pragma asm
    MOV C,P
    CLR A
    RLC A
#pragma endasm
    return ACC;
}

/* Fast RDATA phase only: one call samples four LSB-first bytes and returns
 * their parity. NULL consumes all 32 bits without touching IDATA address 0.
 * REGPARMS: one-byte IDATA pointer in R7, parity result in R7.
 * Clobbers A/P, CY, R1..R7; preserves R0, B and DPTR. No internal calls,
 * shared scratch, bank switching or interrupt masking. Slow mode stays on
 * the existing byte path; direction and the wire parity bit stay in SW_DP.
 */
#if DAP_SWD_FUSED_READ
/* Fused variant also samples wire parity and generates the final turnaround.
 * REGPARMS: pointer R7, turnaround R5 (1..4); return parity mismatch in R7.
 * R7 holds turnaround until the return; B and direction stay unchanged.
 * DPL/DPH hold pointer/count at entry; the fused variant clobbers DPTR.
 */
unsigned char SWD_SharedReadWord(unsigned char idata *datas, unsigned char turnaround)
#else
unsigned char SWD_SharedReadWord(unsigned char idata *datas)
#endif
{
#if DAP_SWD_FUSED_READ
    DPL = (unsigned char)datas;
    DPH = turnaround;
#else
    ACC = (unsigned char)datas;
#endif
#pragma asm
#if DAP_SWD_FUSED_READ
    MOV A,DPL
#endif
    MOV R1,A
    MOV R2,A            ; Original pointer, zero means discard all bytes.
#if DAP_SWD_FUSED_READ
    MOV R7,DPH          ; Save count before R5 becomes the sampled byte.
#endif
    MOV R3,#0           ; Accumulated byte XOR.
    MOV R4,#4
shared_word_byte:
    MOV R6,#8
shared_word_bit:
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    RRC A
    SETB PIN_SWCLK
    DJNZ R6,shared_word_bit
    MOV R5,A
    XRL A,R3
    MOV R3,A
    MOV A,R2
    JZ shared_word_discard
    MOV A,R5
    MOV @R1,A
    INC R1
shared_word_discard:
    DJNZ R4,shared_word_byte
    MOV A,R3
    MOV C,P
    CLR A
    RLC A
#if DAP_SWD_FUSED_READ
    MOV R3,A
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    CLR A
    RLC A
    XRL A,R3
    MOV R3,A            ; Zero means valid parity, one means mismatch.
    SETB PIN_SWCLK
shared_word_turn:
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R7,shared_word_turn
    MOV A,R3
#endif
#pragma endasm
    return ACC;
}

/* Low nibble: APnDP/RnW/A2/A3. P is sampled before inserting parity.
 * Rotate places the request in bits 1..4 and parity in bit 5;
 * Start/Park are one, Stop is zero. Upper request flags are ignored.
 */
unsigned char SWD_SharedHeader(unsigned char request)
{
    ACC = request;
#pragma asm
    ANL A,#00FH
    MOV C,P
    MOV ACC.4,C
    RL A
    ORL A,#081H
#pragma endasm
    return ACC;
}

#if DAP_SWD_FUSED_REQUEST
/* Fast request: compute and send without 16-bit delay setup/dispatch.
 * Eight GPIO clocks match SharedHeader + SharedWrite(0).
 * Clobbers A/P, CY, R6/R7; no bank/IRQ/direction changes.
 */
void SWD_SharedRequest(unsigned char request)
{
    ACC = request;
#pragma asm
    ANL A,#00FH
    MOV C,P
    MOV ACC.4,C
    RL A
    ORL A,#081H
    MOV R6,#8
shared_request_bit:
    RRC A
    MOV PIN_SWDIO,C
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_request_bit
#pragma endasm
}
#endif

#if DAP_SWD_FUSED_PARITY
/* Fast SPI write tail: hardware parity of the four-byte XOR, one GPIO bit.
 * GPIO ownership/direction was restored by DAP_SPI_End before this entry.
 * Keep data-before-falling-clock order, matching the existing SW_WRITE_BIT.
 * REGPARMS R7; clobbers A/P/CY only, no calls, scratch or IRQ masking.
 */
void SWD_SharedWriteParity(unsigned char value)
{
    ACC = value;
#pragma asm
    MOV C,P
    MOV PIN_SWDIO,C
    CLR PIN_SWCLK
    SETB PIN_SWCLK
#pragma endasm
}
#endif

/* Fast initial turnaround (1..4 clocks, validated SWD configuration) + ACK.
 * Caller has already released SWDIO through the board/DIR interface.
 * Sample exactly three LSB-first bits, including invalid ACK values; the
 * C state machine still owns OK/WAIT/FAULT/error handling and later turns.
 * REGPARMS: count/result R7. Clobbers A/P, CY, R6/R7 only; no scratch,
 * internal call, GPIO direction change, bank switch or interrupt masking.
 */
#if DAP_SWD_ACK_WRITE_TURN && !DAP_SWD_ACK_WRITE_ONLY
/* Extended REGPARMS: turn R7, request R5. DPL/DPH are call scratch.
 * Only OK writes consume the final input turnaround here; direction stays
 * released. Reads and every other ACK keep their original phase lengths.
 */
unsigned char SWD_SharedAck(unsigned char turnaround, unsigned char request)
#else
unsigned char SWD_SharedAck(unsigned char turnaround)
#endif
{
#if DAP_SWD_ACK_WRITE_TURN && !DAP_SWD_ACK_WRITE_ONLY
    DPL = turnaround;
    DPH = request;
#endif
    ACC = turnaround;
#pragma asm
    MOV R6,A
shared_ack_turn:
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_ack_turn
    CLR A
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.0,C
    SETB PIN_SWCLK
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.1,C
    SETB PIN_SWCLK
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.2,C
    SETB PIN_SWCLK
#if DAP_SWD_ACK_WRITE_TURN && !DAP_SWD_ACK_WRITE_ONLY
    MOV R7,A
    MOV A,DPH
    JB ACC.1,shared_ack_done
    MOV A,R7
    CJNE A,#1,shared_ack_done
    MOV R6,DPL
shared_ack_write_turn:
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_ack_write_turn
shared_ack_done:
    MOV A,R7
#endif
#pragma endasm
    return ACC;
}

#if DAP_SWD_ACK_WRITE_ONLY
/* Write-only fast ACK: R7 retains the input turn count until the result.
 * Only OK consumes the second input turn. Direction stays board-owned.
 * Same R6/R7 clobbers as the original read ACK; no DPTR or RAM scratch.
 */
unsigned char SWD_SharedWriteAck(unsigned char turnaround)
{
    ACC = turnaround;
#pragma asm
    MOV R6,A
shared_write_ack_initial:
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_write_ack_initial
    CLR A
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.0,C
    SETB PIN_SWCLK
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.1,C
    SETB PIN_SWCLK
    CLR PIN_SWCLK
    MOV C,PIN_SWDIO
    MOV ACC.2,C
    SETB PIN_SWCLK
    CJNE A,#1,shared_write_ack_done
    XCH A,R7
    MOV R6,A
    XCH A,R7
shared_write_ack_final:
    CLR PIN_SWCLK
    SETB PIN_SWCLK
    DJNZ R6,shared_write_ack_final
shared_write_ack_done:
#pragma endasm
    return ACC;
}
#endif

/* Internal delay entry. No C call ABI: do not call from C.
 * The internal call owns R2/R3/B, leaves R4/R5/R6 and shifting ACC unchanged.
 * Decrements a genuine 16-bit count, including low-byte underflow.
 */
void SWD_SharedDelayBody(void)
{
#pragma asm
    MOV B,A
    MOV A,R4
    ORL A,R5
    JZ shared_delay_done
    MOV A,R4
    MOV R2,A
    MOV A,R5
    MOV R3,A
shared_delay_loop:
    INC SAFE_MOD
    MOV A,R3
    JNZ shared_delay_low
    DEC R2
shared_delay_low:
    DEC R3
    MOV A,R2
    ORL A,R3
    JNZ shared_delay_loop
shared_delay_done:
    MOV A,B
#pragma endasm
}
#endif
