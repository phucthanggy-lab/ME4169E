#include "gpio.h"
#include "config.h"
#include <avr/io.h>
#include <util/atomic.h>

void gpio_motor_high_z(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        DDRB &= (uint8_t)~MOTOR_PIN_MASK;
        PORTB &= (uint8_t)~MOTOR_PIN_MASK;
    }
}

void gpio_init(void)
{
    gpio_motor_high_z();
    DDRC &= (uint8_t)~(_BV(PC0) | _BV(PC1) | _BV(PC2));
    PORTC &= (uint8_t)~(_BV(PC0) | _BV(PC1) | _BV(PC2));
#if CFG_BUTTON_INTERNAL_PULLUPS
    PORTC |= _BV(PC1) | _BV(PC2);
#endif
    DDRD &= (uint8_t)~(_BV(PD2) | _BV(PD3));
    PORTD &= (uint8_t)~(_BV(PD2) | _BV(PD3));
#if CFG_ENCODER_INTERNAL_PULLUPS
    PORTD |= _BV(PD2) | _BV(PD3);
#endif
}

bool gpio_run_pressed(void) { return (PINC & _BV(PC1)) == 0; }
bool gpio_stop_pressed(void) { return (PINC & _BV(PC2)) == 0; }

void gpio_motor_direction(uint8_t direction)
{
#if CFG_MOTOR_OUTPUTS_ENABLE
    const uint8_t mask = _BV(PB0) | _BV(PB2);
    uint8_t active = direction == 1U ? _BV(PB0) : (direction == 2U ? _BV(PB2) : 0U);
    uint8_t levels;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
#if CFG_DIRECTION_ACTIVE_LEVEL == 1
        levels = active;
#else
        levels = (uint8_t)(mask & (uint8_t)~active);
#endif
        PORTB = (uint8_t)((PORTB & (uint8_t)~mask) | levels);
        DDRB |= mask; /* Inactive/selected values are loaded before driving pins. */
    }
#else
    (void)direction;
    gpio_motor_high_z();
#endif
}
