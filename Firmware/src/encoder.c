#include "encoder.h"
#include "control.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <limits.h>
static volatile uint32_t position;
static volatile int32_t window_delta;
static volatile uint16_t invalid_edges;
static volatile bool overflowed;
static volatile bool moved;
static volatile bool bad_transition;
static uint8_t previous;
static uint8_t read_state(void) { return (uint8_t)((PIND >> 2U) & 3U); }

void encoder_init(void)
{
    GICR &= (uint8_t)~(_BV(INT0) | _BV(INT1));
    position = 0;
    window_delta = 0;
    invalid_edges = 0;
    overflowed = false;
    moved = false;
    bad_transition = false;
    previous = read_state();
#if CFG_ENCODER_MODE == CFG_ENCODER_QUADRATURE_X4
    MCUCR = (uint8_t)((MCUCR & (uint8_t)~(_BV(ISC01) | _BV(ISC00) | _BV(ISC11) | _BV(ISC10))) | _BV(ISC00) | _BV(ISC10));
#else
    MCUCR = (uint8_t)((MCUCR & (uint8_t)~(_BV(ISC01) | _BV(ISC00) | _BV(ISC11) | _BV(ISC10))) | _BV(ISC01) | _BV(ISC00));
#endif
    GIFR = _BV(INTF0) | _BV(INTF1);
#if CFG_ENCODER_MODE == CFG_ENCODER_QUADRATURE_X4
    GICR |= _BV(INT0) | _BV(INT1);
#else
    GICR |= _BV(INT0);
#endif
}

static void record_step(int8_t change)
{
    if (change != 0) { moved = true; }
    if (change > 0) {
        position++; /* Defined modulo-2^32 position, never signed overflow. */
        if (window_delta == INT32_MAX) { overflowed = true; }
        else { window_delta++; }
    } else if (change < 0) {
        position--;
        if (window_delta == INT32_MIN) { overflowed = true; }
        else { window_delta--; }
    }
}

void encoder_process_state(uint8_t state)
{
    int8_t change;
    state &= 3U;
#if CFG_ENCODER_MODE == CFG_ENCODER_QUADRATURE_X4
    if ((previous ^ state) == 3U) {
        bad_transition = true;
        if (invalid_edges < UINT16_MAX) { invalid_edges++; }
    }
    change = control_quadrature(previous, state);
#if CFG_ENCODER_REVERSED
    change = (int8_t)-change;
#endif
#else
    change = (previous & 1U) == 0U && (state & 1U) != 0U ? 1 : 0;
#endif
    previous = state;
    record_step(change);
}

#if CFG_ENCODER_MODE == CFG_ENCODER_QUADRATURE_X4
ISR(INT0_vect) { encoder_process_state(read_state()); }
ISR(INT1_vect) { encoder_process_state(read_state()); }
#else
ISR(INT0_vect) { record_step(1); } /* Rising edge; magnitude only, no direction. */
#endif

encoder_sample_t encoder_snapshot(void)
{
    encoder_sample_t result;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result.position = position;
        result.delta = window_delta;
        result.invalid = invalid_edges;
        result.overflow = overflowed;
        result.moving = moved;
        result.invalid_transition = bad_transition;
        window_delta = 0;
        overflowed = false;
        moved = false;
        bad_transition = false;
    }
    return result;
}
