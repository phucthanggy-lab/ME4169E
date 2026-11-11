#include "motor.h"
#include "pwm.h"
#include "gpio.h"
#include "control.h"
#include "config.h"
static motor_state_t state;
static motor_fault_t fault;
static motor_direction_t pending_direction;
static motor_direction_t last_direction;
static uint32_t coast_started;
static uint32_t last_motion;
static uint32_t last_service;
static bool outputs_ready;

bool motor_outputs_available(void) { return outputs_ready; }
motor_state_t motor_state(void) { return state; }
motor_fault_t motor_fault(void) { return fault; }

static void deactivate(void)
{
    pwm_disable(); /* Always remove PWM before changing either direction line. */
    gpio_motor_direction(MOTOR_DIR_NONE);
}

void motor_init(uint32_t now)
{
    state = MOTOR_STOPPED;
    fault = MOTOR_FAULT_NONE;
    pending_direction = MOTOR_DIR_NONE;
    last_direction = MOTOR_DIR_NONE;
    coast_started = now;
    last_motion = now;
    last_service = now;
    outputs_ready = pwm_init();
    deactivate();
}

void motor_raise_fault(motor_fault_t reason)
{
    deactivate();
    pending_direction = MOTOR_DIR_NONE;
    state = MOTOR_FAULT;
    if (fault == MOTOR_FAULT_NONE) { fault = reason; } /* Preserve first cause. */
}

void motor_stop(uint32_t now)
{
    deactivate();
    pending_direction = MOTOR_DIR_NONE;
    last_service = now;
    if (state != MOTOR_FAULT) { state = MOTOR_STOPPED; }
}

bool motor_request(motor_direction_t direction, uint32_t now)
{
    if (state == MOTOR_FAULT) { return false; }
    if (!outputs_ready || (direction != MOTOR_DIR_FORWARD && direction != MOTOR_DIR_BACKWARD)) {
        motor_raise_fault(MOTOR_FAULT_CONFIG);
        return false;
    }
    if (state == MOTOR_COASTING) { pending_direction = direction; return true; }
    if ((state == MOTOR_RUN_FORWARD && direction == MOTOR_DIR_FORWARD) ||
        (state == MOTOR_RUN_BACKWARD && direction == MOTOR_DIR_BACKWARD)) { return true; }
    if (state == MOTOR_RUN_FORWARD || state == MOTOR_RUN_BACKWARD ||
        (last_direction != MOTOR_DIR_NONE && direction != last_direction)) {
        deactivate();
#if CFG_REVERSAL_SAFETY_CONFIRMED && CFG_REVERSAL_DEADTIME_MS > 0 && CFG_REVERSAL_QUIET_MS > 0 && CFG_REVERSAL_TIMEOUT_MS > 0
        pending_direction = direction;
        coast_started = now;
        last_motion = now;
        state = MOTOR_COASTING;
        return true;
#else
        motor_raise_fault(MOTOR_FAULT_REVERSAL);
        return false;
#endif
    }
    pwm_disable();
    gpio_motor_direction((uint8_t)direction);
    last_direction = direction;
    state = direction == MOTOR_DIR_FORWARD ? MOTOR_RUN_FORWARD : MOTOR_RUN_BACKWARD;
    last_service = now;
    return true;
}

void motor_task(uint32_t now, uint16_t command, bool adc_valid, bool encoder_moving)
{
    uint32_t service_gap = (uint32_t)(now - last_service);
    last_service = now;
    if (encoder_moving) { last_motion = now; }
    if (state == MOTOR_STOPPED || state == MOTOR_FAULT) { return; }
    if (service_gap > CFG_MAIN_STALL_MS) { motor_raise_fault(MOTOR_FAULT_SCHEDULER); return; }
    if (!adc_valid) { motor_raise_fault(MOTOR_FAULT_ADC); return; }
    if (state == MOTOR_COASTING) {
        deactivate();
#if CFG_REVERSAL_SAFETY_CONFIRMED && CFG_REVERSAL_DEADTIME_MS > 0 && CFG_REVERSAL_QUIET_MS > 0 && CFG_REVERSAL_TIMEOUT_MS > 0
        if ((uint32_t)(now - coast_started) >= CFG_REVERSAL_TIMEOUT_MS) { motor_raise_fault(MOTOR_FAULT_TIMEOUT); return; }
        if ((uint32_t)(now - coast_started) < CFG_REVERSAL_DEADTIME_MS || (uint32_t)(now - last_motion) < CFG_REVERSAL_QUIET_MS) { return; }
        gpio_motor_direction((uint8_t)pending_direction);
        last_direction = pending_direction;
        state = pending_direction == MOTOR_DIR_FORWARD ? MOTOR_RUN_FORWARD : MOTOR_RUN_BACKWARD;
        pending_direction = MOTOR_DIR_NONE;
#else
        motor_raise_fault(MOTOR_FAULT_REVERSAL);
#endif
        return; /* PWM starts ramping on the next service, after direction is established. */
    }
    if (command > CFG_MOTOR_DUTY_LIMIT_PERMILLE) { command = CFG_MOTOR_DUTY_LIMIT_PERMILLE; }
    if (!pwm_apply(control_ramp(pwm_duty(), command, CFG_DUTY_RAMP_STEP_PERMILLE))) { motor_raise_fault(MOTOR_FAULT_CONFIG); }
}
