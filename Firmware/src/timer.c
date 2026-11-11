#include "timer.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
static volatile uint32_t milliseconds;

void timer_init(void)
{
    TCCR2 = 0;
    ASSR &= (uint8_t)~_BV(AS2); /* Synchronous CPU clock, not a guessed 32 kHz crystal. */
    TCNT2 = 0;
    OCR2 = (uint8_t)CFG_TIMER2_OCR;
    milliseconds = 0;
    TIFR = _BV(OCF2);
    TIMSK |= _BV(OCIE2);
    TCCR2 = (uint8_t)(_BV(WGM21) | CFG_TIMER2_CS_BITS); /* COM2x=0: never drive MOSI. */
}

ISR(TIMER2_COMP_vect) { milliseconds++; }

uint32_t timer_millis(void)
{
    uint32_t snapshot;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { snapshot = milliseconds; }
    return snapshot;
}
