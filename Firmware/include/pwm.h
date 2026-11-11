#ifndef PWM_H
#define PWM_H
#include <stdbool.h>
#include <stdint.h>
bool pwm_init(void);
void pwm_disable(void);
bool pwm_apply(uint16_t duty_per_mille);
uint16_t pwm_duty(void);
uint16_t pwm_top(void);
uint16_t pwm_prescaler(void);
#endif
