#ifndef MOTOR_H
#define MOTOR_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { MOTOR_STOPPED, MOTOR_RUN_FORWARD, MOTOR_RUN_BACKWARD, MOTOR_COASTING, MOTOR_FAULT } motor_state_t;
typedef enum { MOTOR_DIR_NONE = 0, MOTOR_DIR_FORWARD = 1, MOTOR_DIR_BACKWARD = 2 } motor_direction_t;
typedef enum { MOTOR_FAULT_NONE, MOTOR_FAULT_CONFIG, MOTOR_FAULT_ADC, MOTOR_FAULT_ENCODER, MOTOR_FAULT_REVERSAL, MOTOR_FAULT_TIMEOUT, MOTOR_FAULT_SCHEDULER } motor_fault_t;
void motor_init(uint32_t now);
bool motor_request(motor_direction_t direction, uint32_t now);
void motor_stop(uint32_t now);
void motor_raise_fault(motor_fault_t fault);
void motor_task(uint32_t now, uint16_t command, bool adc_valid, bool encoder_moving);
motor_state_t motor_state(void);
motor_fault_t motor_fault(void);
bool motor_outputs_available(void);
#endif
