#include "control.h"
#include "config.h"
#include <limits.h>
#include <avr/pgmspace.h>

void debounce_init(debounce_t *button, bool pressed, uint32_t now)
{
    button->stable = pressed;
    button->candidate = pressed;
    button->since_ms = now;
}

bool debounce_update(debounce_t *button, bool pressed, uint32_t now, uint32_t interval)
{
    if (pressed != button->candidate) {
        button->candidate = pressed;
        button->since_ms = now;
    }
    if (button->stable != button->candidate && (uint32_t)(now - button->since_ms) >= interval) {
        button->stable = button->candidate;
        return button->stable; /* Only a new stable press produces an event. */
    }
    return false;
}

uint16_t control_adc_duty(uint16_t adc)
{
    if (adc > CFG_ADC_FULL_SCALE) { adc = CFG_ADC_FULL_SCALE; }
    return (uint16_t)(((uint32_t)adc * CFG_DUTY_SCALE + CFG_ADC_FULL_SCALE / 2U) / CFG_ADC_FULL_SCALE);
}

uint16_t control_ramp(uint16_t actual, uint16_t target, uint16_t step)
{
    if (actual < target) { return (uint16_t)(target - actual) <= step ? target : (uint16_t)(actual + step); }
    return (uint16_t)(actual - target) <= step ? target : (uint16_t)(actual - step);
}

bool control_rpm(int32_t counts, uint32_t counted_cpr, uint32_t elapsed_ms, int32_t *rpm)
{
    uint32_t magnitude;
    uint64_t denominator;
    uint64_t result;
    if (counted_cpr == 0UL || elapsed_ms == 0UL || rpm == 0) { return false; }
    magnitude = counts < 0 ? 0UL - (uint32_t)counts : (uint32_t)counts;
    denominator = (uint64_t)counted_cpr * elapsed_ms;
    /* Max numerator ~1.29e14. Avoid adding denominator/2, which can overflow for
     * arbitrary 32-bit input products: quotient/remainder rounding is bounded. */
    result = (uint64_t)magnitude * 60000UL;
    {
        uint64_t quotient = result / denominator;
        uint64_t remainder = result % denominator;
        if (remainder >= denominator / 2ULL + denominator % 2ULL) { quotient++; }
        result = quotient;
    }
    if (result > INT32_MAX) { return false; }
    *rpm = counts < 0 ? -(int32_t)result : (int32_t)result;
    return true;
}

bool control_pwm_plan(uint32_t clock_hz, uint32_t frequency_hz, uint16_t *prescaler, uint16_t *top)
{
    static const uint16_t dividers[] PROGMEM = {1U, 8U, 64U, 256U, 1024U};
    uint8_t index;
    if (clock_hz == 0 || clock_hz > 16000000UL || frequency_hz == 0 || prescaler == 0 || top == 0) { return false; }
    for (index = 0; index < 5U; index++) {
        uint16_t divider = pgm_read_word(&dividers[index]);
        uint32_t denominator;
        uint32_t period;
        if (frequency_hz > UINT32_MAX / divider) { continue; }
        denominator = frequency_hz * divider;
        period = (clock_hz + denominator / 2UL) / denominator;
        if (period >= 4UL && period <= 65536UL) {
            *prescaler = divider;
            *top = (uint16_t)(period - 1UL);
            return true;
        }
    }
    return false;
}

int8_t control_quadrature(uint8_t previous, uint8_t current)
{
    /* State = (B << 1) | A. + sequence: 00,01,11,10,00. Wiring defines physical sign. */
    static const int8_t table[16] PROGMEM = {0,1,-1,0, -1,0,0,1, 1,0,0,-1, 0,-1,1,0};
    return (int8_t)pgm_read_byte(&table[((previous & 3U) << 2U) | (current & 3U)]);
}
