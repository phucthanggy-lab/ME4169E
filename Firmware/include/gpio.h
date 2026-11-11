#ifndef GPIO_H
#define GPIO_H
#include <stdbool.h>
#include <stdint.h>
#define MOTOR_PIN_MASK ((uint8_t)((1U << 0) | (1U << 1) | (1U << 2)))
void gpio_init(void);
bool gpio_run_pressed(void);
bool gpio_stop_pressed(void);
void gpio_motor_direction(uint8_t direction);
void gpio_motor_high_z(void);
#endif
