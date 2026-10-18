; CH55x compact clock arithmetic. Standard MCS-51 instructions only.
; C51 ABI: operand / result R4:R5:R6:R7, MSB first.
; Operand R4: 0=floor(12000000/d), 1/2=UART nearest, 3=SWJ calibrated.
; Divisor R5:R6:R7 is validated by the caller (see CH55x_Math.H).
; Clobbers A, B, CY, DPTR, R0..R7 of the CURRENT register bank.
; No DATA/XDATA/static parameters. No calls. Additional stack: 0 or 5 bytes.
; Interrupt/task callers must preserve the normal C51 volatile registers.
; Do not replace register operands with absolute ARn bank-zero addresses.

NAME CH55X_MATH
?PR?_CH55x_ClockDivide?CH55X_MATH SEGMENT CODE
PUBLIC _CH55x_ClockDivide
RSEG ?PR?_CH55x_ClockDivide?CH55X_MATH

_CH55x_ClockDivide:
    MOV DPH,R4
    MOV A,R7
    MOV R0,A
    MOV A,R6
    MOV R1,A
    MOV A,R5
    MOV R2,A
    MOV A,DPH
    JZ swj_base
    CJNE A,#3,select_uart_base
    SJMP swj_base
select_uart_base:
    CJNE A,#1,slow_base
    MOV R5,#016H
    MOV R6,#0E3H
    MOV R7,#060H
    SJMP divide_start
slow_base:
    MOV R5,#005H
    MOV R6,#0B8H
    MOV R7,#0D8H
    SJMP divide_start
swj_base:
    MOV R5,#0B7H
    MOV R6,#01BH
    MOV R7,#000H
divide_start:
    MOV R3,#0
    MOV R4,#0
    MOV B,#0
    MOV DPL,#24
divide_bit:
    CLR C
    MOV A,R7
    RLC A
    MOV R7,A
    MOV A,R6
    RLC A
    MOV R6,A
    MOV A,R5
    RLC A
    MOV R5,A
    MOV A,R3
    RLC A
    MOV R3,A
    MOV A,R4
    RLC A
    MOV R4,A
    MOV A,B
    RLC A
    MOV B,A
    CLR C
    MOV A,R3
    SUBB A,R0
    MOV A,R4
    SUBB A,R1
    MOV A,B
    SUBB A,R2
    JC divide_next
    CLR C
    MOV A,R3
    SUBB A,R0
    MOV R3,A
    MOV A,R4
    SUBB A,R1
    MOV R4,A
    MOV A,B
    SUBB A,R2
    MOV B,A
    INC R7
divide_next:
    DJNZ DPL,divide_bit
    MOV A,DPH
    JZ floor_done
    CJNE A,#3,not_calibrated
    LJMP calibrate_count
not_calibrated:
    CJNE A,#4,round_count
    ; Internal second pass: floor((ceil(12 MHz/d)-11)/14).
    MOV A,R5
    ORL A,R6
    ORL A,R7
    JNZ floor_done
    INC R7
floor_done:
    MOV R4,#0
    RET

calibrate_count:
    ; Round the first quotient up, so integer truncation cannot speed up
    ; the requested clock. Use the remainder already in B:R4:R3.
    MOV A,R3
    ORL A,R4
    ORL A,B
    JZ calibrate_subtract
    INC R7
    MOV A,R7
    JNZ calibrate_subtract
    INC R6
    MOV A,R6
    JNZ calibrate_subtract
    INC R5
calibrate_subtract:
    ; ceil((half_ticks-24)/14) = floor((half_ticks-11)/14).
    ; A positive count is mandatory: zero selects the fast SPI backend.
    CLR C
    MOV A,R7
    SUBB A,#11
    MOV R7,A
    MOV A,R6
    SUBB A,#0
    MOV R6,A
    MOV A,R5
    SUBB A,#0
    MOV R5,A
    JNC calibrate_divide
    MOV R5,#0
    MOV R6,#0
    MOV R7,#1
    SJMP floor_done
calibrate_divide:
    MOV R0,#14
    MOV R1,#0
    MOV R2,#0
    MOV DPH,#4
    LJMP divide_start

round_count:
    ; q is at most 65217. Save q, then threshold A-r, high byte first.
    MOV A,R7
    PUSH ACC
    MOV A,R6
    PUSH ACC
    MOV A,DPH
    CJNE A,#1,slow_threshold
    MOV R0,#060H
    MOV R1,#0E3H
    MOV R2,#016H
    SJMP threshold
slow_threshold:
    MOV R0,#0D8H
    MOV R1,#0B8H
    MOV R2,#005H
threshold:
    CLR C
    MOV A,R0
    SUBB A,R3
    MOV R0,A
    MOV A,R1
    SUBB A,R4
    MOV R1,A
    MOV A,R2
    SUBB A,B
    PUSH ACC
    MOV A,R1
    PUSH ACC
    MOV A,R0
    PUSH ACC
    ; Compare r*(2*q+1) > A-r. Product is bounded by 2*A+r (<2^24).
    CLR C
    MOV A,R7
    RLC A
    ORL A,#1
    MOV R7,A
    MOV A,R6
    RLC A
    MOV R6,A
    MOV A,R5
    RLC A
    MOV R5,A
    MOV R0,#0
    MOV R1,#0
    MOV R2,#0
multiply_bit:
    MOV A,R7
    JNB ACC.0,multiply_shift
    MOV A,R0
    ADD A,R3
    MOV R0,A
    MOV A,R1
    ADDC A,R4
    MOV R1,A
    MOV A,R2
    ADDC A,B
    MOV R2,A
multiply_shift:
    CLR C
    MOV A,R3
    RLC A
    MOV R3,A
    MOV A,R4
    RLC A
    MOV R4,A
    MOV A,B
    RLC A
    MOV B,A
    CLR C
    MOV A,R5
    RRC A
    MOV R5,A
    MOV A,R6
    RRC A
    MOV R6,A
    MOV A,R7
    RRC A
    MOV R7,A
    ORL A,R6
    ORL A,R5
    JNZ multiply_bit
    CLR C
    POP ACC
    SUBB A,R0
    POP ACC
    SUBB A,R1
    POP ACC
    SUBB A,R2
    ; CY=1 iff product > threshold. Equality keeps the smaller divisor.
    POP ACC
    MOV R6,A
    POP ACC
    MOV R7,A
    JNC round_done
    INC R7
    MOV A,R7
    JNZ round_done
    INC R6
round_done:
    MOV R5,#0
    MOV R4,#0
    RET
END
