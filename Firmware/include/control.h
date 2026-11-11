#ifndef CONTROL_H
#define CONTROL_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool stable; bool candidate; uint32_t since_ms; } debounce_t;
void debounce_init(debounce_t *button, bool pressed, uint32_t now);
bool debounce_update(debounce_t *button, bool pressed, uint32_t now, uint32_t interval);
uint16_t control_adc_duty(uint16_t adc);
uint16_t control_ramp(uint16_t actual, uint16_t target, uint16_t step);
bool control_rpm(int32_t counts, uint32_t counted_cpr, uint32_t elapsed_ms, int32_t *rpm);
bool control_pwm_plan(uint32_t clock_hz, uint32_t frequency_hz, uint16_t *prescaler, uint16_t *top);
int8_t control_quadrature(uint8_t previous, uint8_t current);
#endif
