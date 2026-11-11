#include "pwm.h"
#include "control.h"
#include "config.h"
#include "gpio.h"
#include <avr/io.h>
#include <util/atomic.h>
static bool configured;
static uint16_t configured_top;
static uint16_t divider;
static uint16_t applied_duty;

static void output_level(bool active)
{
#if CFG_MOTOR_OUTPUTS_ENABLE
    bool high = active == (CFG_PWM_ACTIVE_LEVEL == 1);
    if (high) { PORTB |= _BV(PB1); } else { PORTB &= (uint8_t)~_BV(PB1); }
    DDRB |= _BV(PB1);
#else
    (void)active;
    gpio_motor_high_z();
#endif
}

void pwm_disable(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        /* Preload the inactive GPIO latch while compare still owns OC1A.
         * Otherwise a latch left active by 100% duty can reappear at disconnect. */
        output_level(false);
        TCCR1A &= (uint8_t)~(_BV(COM1A1) | _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0));
        applied_duty = 0;
    }
}

bool pwm_init(void)
{
    configured = false;
    configured_top = 0;
    divider = 0;
    pwm_disable();
    TCCR1A = 0;
    TCCR1B = 0;
#if CFG_MOTOR_OUTPUTS_ENABLE
    if (control_pwm_plan(F_CPU, CFG_PWM_TARGET_HZ, &divider, &configured_top)) {
        uint8_t clock_bits = divider == 1U ? 1U : (divider == 8U ? 2U : (divider == 64U ? 3U : (divider == 256U ? 4U : 5U)));
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            TCNT1 = 0;
            ICR1 = configured_top;
            OCR1A = 0;
            /* Fast PWM mode 14, TOP=ICR1. COM1B remains zero; PB2 is direction. */
            TCCR1A = _BV(WGM11);
            TCCR1B = (uint8_t)(_BV(WGM13) | _BV(WGM12) | clock_bits);
        }
        configured = true;
    }
#endif
    return configured;
}

bool pwm_apply(uint16_t duty_per_mille)
{
    if (!configured) { pwm_disable(); return false; }
    if (duty_per_mille > CFG_MOTOR_DUTY_LIMIT_PERMILLE) { duty_per_mille = CFG_MOTOR_DUTY_LIMIT_PERMILLE; }
    if (duty_per_mille == 0U) { pwm_disable(); return true; }
    {
        uint32_t period = (uint32_t)configured_top + 1UL;
        uint32_t high_counts = (period * duty_per_mille + CFG_DUTY_SCALE / 2U) / CFG_DUTY_SCALE;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            if (high_counts == 0) {
                output_level(false);
                TCCR1A &= (uint8_t)~(_BV(COM1A1) | _BV(COM1A0));
            } else if (high_counts >= period) {
                output_level(true);
                TCCR1A &= (uint8_t)~(_BV(COM1A1) | _BV(COM1A0));
            } else {
                OCR1A = (uint16_t)(high_counts - 1UL);
                TCCR1A = (uint8_t)((TCCR1A & (uint8_t)~(_BV(COM1A1) | _BV(COM1A0))) | _BV(COM1A1)
#if CFG_PWM_ACTIVE_LEVEL == 0
                    | _BV(COM1A0)
#endif
                );
            }
            applied_duty = duty_per_mille;
        }
    }
    return true;
}

uint16_t pwm_duty(void) { return applied_duty; }
uint16_t pwm_top(void) { return configured_top; }
uint16_t pwm_prescaler(void) { return divider; }
