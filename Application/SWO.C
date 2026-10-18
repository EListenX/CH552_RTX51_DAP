/* CH552 UART1 SWO/NRZ capture, transported by existing DAP_SWO_Data.
 * No USB endpoint, CDC buffer, DMA layout or timer ownership changes.
 * The 128-byte ring has 127 usable slots; overflow drops NEW bytes.
 */
#include "SWO.H"
#include "IO_Config.H"
#include "CH55x_Math.H"

#if SWO_UART
#if DAP_GPIO_SWJ_SWO
#pragma OPTIMIZE(9, SIZE)
#else
#pragma OPTIMIZE(8, SIZE)
#endif
#define SWO_RING_MASK (SWO_BUFFER_SIZE - 1U)
#define SWO_STATUS_SIZE 5U
#define SWO_DATA_HEADER_SIZE 3U
static UINT8 xdata trace_buffer[SWO_BUFFER_SIZE];
static volatile UINT8 idata trace_write;
static volatile UINT8 idata trace_read;
static volatile UINT8 idata trace_error;
static UINT8 idata trace_active;
static UINT8 idata trace_transport;
static UINT8 idata trace_mode;
static UINT8 idata trace_ready;

void SWO_Reset(void)
{
    SWO_USB_RESET();
    U1RI = 0; U1TI = 0;
    trace_write = trace_read = trace_error = 0U;
    trace_active = trace_transport = trace_mode = trace_ready = 0U;
}

/* UART1 has no hardware FIFO: service once per byte, without task signals.
 * Store payload BEFORE publishing write index; never overwrite unread data.
 */
/* Bank 2 holds persistent RTX timer state, bank 3 is high-priority UART0.
 * UART1 and USB share bank 1 at the same (low) priority, so cannot nest.
 */
void SWO_UART1_ISR(void) interrupt INT_NO_UART1 using 1
{
    UINT8 value, next;
    if (U1RI) {
        value = SBUF1;
        U1RI = 0;
        next = (trace_write + 1U) & SWO_RING_MASK;
        if (!U1RB8) { trace_error |= DAP_SWO_STREAM_ERROR; }
        else if (next == trace_read) { trace_error |= DAP_SWO_BUFFER_OVERRUN; }
        else { trace_buffer[trace_write] = value; trace_write = next; }
    }
    U1TI = 0;
}

/* Error read/clear must not race ISR OR; gate ONLY UART1, preserve its state. */
static UINT8 SWO_Status(void)
{
    UINT8 irq, status;
    irq = IE_UART1;
    IE_UART1 = 0;
    status = trace_active | trace_error;
    trace_error = 0U;
    IE_UART1 = irq;
    return status;
}

static UINT8 SWO_Count(void)
{
    return (trace_write - trace_read) & SWO_RING_MASK;
}

/* Boundary-only arithmetic. UART1 fast clock = 1500000 / N, N=1..256.
 * Report the achieved integer baud; host programs the target accordingly.
 * Reuse compact division, do not link C51 long multiplication/division.
 */
static UINT32 SWO_Baudrate(UINT32 requested) DAP_SWO_MEMORY
{
    UINT16 count;
    UINT32 actual;
    if (!trace_mode || requested < 5860UL) { return 0UL; }
    if (requested > SWO_UART_MAX_BAUDRATE) { requested = SWO_UART_MAX_BAUDRATE; }
#if DAP_GPIO_SWJ_SWO
    /* Accepted 5860..1500000 gives quotient 8..2047 and count 1..256. */
    count = ((UINT16)CH55x_ClockDivide(requested) + 4U) >> 3;
    actual = CH55x_ClockDivide((UINT16)(count << 3));
#else
    count = (UINT16)((CH55x_ClockDivide(requested) + 4UL) >> 3);
    if (count == 0U) { count = 1U; }
    if (count > 256U) { count = 256U; }
    actual = CH55x_ClockDivide((UINT32)count << 3);
#endif
    IE_UART1 = 0; U1REN = 0;
    SBAUD1 = (UINT8)(0U - count);
    U1SMOD = 1;
    U1RI = 0;
    trace_ready = 1U;
    if (trace_active) { U1REN = 1; IE_UART1 = 1; }
    return actual;
}

/* Fixed request/response headers checked by DAP dispatcher. Variable data
 * and ExtendedStatus are checked here before read-clear or ring consumption.
 * Returns 8-bit payload length. ExtendedStatus flags=0 has a valid empty
 * payload; dispatcher checks its capacity before interpreting this result.
 */
UINT8 SWO_Command(UINT8 command, const UINT8 xdata *req,
                  UINT8 xdata *resp, UINT8 capacity) DAP_SWO_MEMORY
{
    UINT8 value, count, n, length;
#if DAP_GPIO_SWJ_SWO
    /* C51 stores 32-bit integers little-endian, as required by CMSIS-DAP. */
    union { UINT32 word; UINT8 bytes[4]; } baud;
#else
    UINT32 baud;
#endif
    value = (command == ID_DAP_SWO_Status) ? 0U : req[0];
    switch (command) {
    case ID_DAP_SWO_Transport:
        if (trace_active || value > 1U) { resp[0] = DAP_ERROR; }
        else { trace_transport = value; resp[0] = DAP_OK; }
        return 1U;
    case ID_DAP_SWO_Mode:
        n = trace_transport;
        SWO_Reset();
        /* Mode changes keep the transport choice, as specified by Arm. */
        trace_transport = n;
        if (value > DAP_SWO_UART) { resp[0] = DAP_ERROR; return 1U; }
        trace_mode = value;
        if (value) {
            PIN_FUNC |= bUART1_PIN_X;
            P3 |= PIN_SWO_MASK;
            P3_MOD_OC &= ~PIN_SWO_MASK;
            P3_DIR_PU &= ~PIN_SWO_MASK;
            U1SM0 = 0;
            IP_EX &= ~bIP_UART1;
        }
        resp[0] = DAP_OK;
        return 1U;
    case ID_DAP_SWO_Baudrate:
#if DAP_GPIO_SWJ_SWO
        for (n = 0U; n < 4U; n++) { baud.bytes[n] = req[n]; }
        baud.word = SWO_Baudrate(baud.word);
        if (!baud.word) { IE_UART1 = 0; U1REN = 0; trace_active = trace_ready = 0U; }
        for (n = 0U; n < 4U; n++) { resp[n] = baud.bytes[n]; }
#else
        baud = (UINT32)req[0] | ((UINT32)req[1] << 8) |
               ((UINT32)req[2] << 16) | ((UINT32)req[3] << 24);
        baud = SWO_Baudrate(baud);
        if (!baud) { IE_UART1 = 0; U1REN = 0; trace_active = trace_ready = 0U; }
        for (n = 0U; n < 4U; n++) { resp[n] = (UINT8)baud; baud >>= 8; }
#endif
        return 4U;
    case ID_DAP_SWO_Control:
        value &= DAP_SWO_CAPTURE_ACTIVE;
#if DAP_GPIO_SWJ_SWO
        if (value && DAP_Data.debug_port == DAP_PORT_JTAG) { resp[0] = DAP_ERROR; return 1U; }
#endif
        if (value && (!trace_mode || !trace_ready)) { resp[0] = DAP_ERROR; }
        else {
            if (value != trace_active) {
                IE_UART1 = 0; U1REN = 0;
                if (value) { trace_write = trace_read = trace_error = 0U; U1RI = 0; }
                trace_active = value;
                if (value) { U1REN = 1; IE_UART1 = 1; }
            }
            resp[0] = DAP_OK;
        }
        return 1U;
    case ID_DAP_SWO_Status:
        resp[0] = SWO_Status();
        resp[1] = SWO_Count();
        resp[2] = resp[3] = resp[4] = 0U;
        return SWO_STATUS_SIZE;
    case ID_DAP_SWO_ExtendedStatus:
        length = ((value & 1U) ? 1U : 0U) + ((value & 2U) ? 4U : 0U);
        if (capacity < length) { return 0U; }
        if (value & 1U) { *resp++ = SWO_Status(); }
        if (value & 2U) { *resp++ = SWO_Count(); *resp++ = 0U; *resp++ = 0U; *resp = 0U; }
        /* No timestamp capability. Empty payload still needs a valid command ID. */
        return length;
    case ID_DAP_SWO_Data:
        if (capacity < SWO_DATA_HEADER_SIZE) { return 0U; }
        count = SWO_Count();
        n = capacity - SWO_DATA_HEADER_SIZE;
        if (!req[1] && req[0] < n) { n = req[0]; }
        if (count > n) { count = n; }
        if (trace_transport != 1U) { count = 0U; }
        *resp++ = SWO_Status();
        *resp++ = count; *resp++ = 0U;
        for (n = count; n; n--) {
            *resp++ = trace_buffer[trace_read];
            trace_read = (trace_read + 1U) & SWO_RING_MASK;
        }
        return SWO_DATA_HEADER_SIZE + count;
    default:
        return 0U;
    }
}
#endif
