#include "config.h"
#include "gpio.h"
#include "timer.h"
#include "adc.h"
#include "encoder.h"
#include "lcd.h"
#include "motor.h"
#include "pwm.h"
#include "control.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

volatile uint8_t firmware_reset_cause __attribute__((section(".noinit")));
void firmware_early_init(void) __attribute__((naked, section(".init3"), used));
void firmware_early_init(void)
{
    firmware_reset_cause = MCUCSR;
    MCUCSR &= (uint8_t)~_BV(WDRF);
    wdt_disable();
}

static void render(int32_t rpm, bool rpm_valid, uint16_t duty, uint8_t encoder_bad, uint32_t count)
{
    char first[17];
    char second[17];
    static char previous_first[17];
    static char previous_second[17];
    const char *label;
    if (!motor_outputs_available()) { label = motor_state() == MOTOR_FAULT ? "LOCK!" : "LOCK"; }
    else if (motor_state() == MOTOR_RUN_FORWARD) { label = "RUN F"; }
    else if (motor_state() == MOTOR_RUN_BACKWARD) { label = "RUN B"; }
    else if (motor_state() == MOTOR_COASTING) { label = "COAST"; }
    else if (motor_state() == MOTOR_FAULT) { label = "FAULT"; }
    else { label = "STOP"; }
    (void)snprintf(first, sizeof(first), "%-5s SET:%3u.%u", label, duty / 10U, duty % 10U);
    if (encoder_bad) { (void)snprintf(second, sizeof(second), "RPM: ENC? E:%u", (unsigned)motor_fault()); }
    else if (!rpm_valid) {
#if CFG_ENCODER_COUNTS_PER_MEASURED_REV == 0
        (void)snprintf(second, sizeof(second), "C:%10lu E:%u", (unsigned long)count, (unsigned)motor_fault());
#else
        (void)snprintf(second, sizeof(second), "RPM: ---- E:%u", (unsigned)motor_fault());
#endif
    }
    else if (rpm > 999999L || rpm < -99999L) { (void)snprintf(second, sizeof(second), "RPM: OVER RANGE"); }
    else { (void)snprintf(second, sizeof(second), "RPM:%7ld E:%u", (long)rpm, (unsigned)motor_fault()); }
    (void)count;
    if (strcmp(first, previous_first) != 0) { lcd_write_line(0U, first); strcpy(previous_first, first); }
    if (strcmp(second, previous_second) != 0) { lcd_write_line(1U, second); strcpy(previous_second, second); }
}

int main(void)
{
    debounce_t run_button;
    debounce_t stop_button;
    uint32_t last_control = 0;
    uint32_t last_lcd = 0;
    uint32_t last_rpm = 0;
    uint32_t encoder_position = 0;
    uint32_t count_origin = 0;
    int32_t accumulated_counts = 0;
    int32_t rpm = 0;
    bool rpm_valid = false;
    bool window_bad = false;
    bool run_armed = false;
    uint8_t displayed_encoder_bad = 0;
    cli();
    gpio_init();
    motor_init(0);
    adc_init();
    debounce_init(&run_button, gpio_run_pressed(), 0);
    debounce_init(&stop_button, gpio_stop_pressed(), 0);
    lcd_init();
#if FW_PROFILE == FW_PROFILE_DEVELOPMENT
    lcd_write_line(0U, "ME4169E DEV");
#elif FW_PROFILE == FW_PROFILE_RELEASE
    lcd_write_line(0U, "ME4169E RELEASE");
#else
    lcd_write_line(0U, "TEST FIXTURE");
#endif
    lcd_write_line(1U, motor_outputs_available() ? "STOPPED" : "MOTOR LOCKED");
    encoder_init(); /* Begin acquisition after blocking LCD startup, then enable IRQs. */
    timer_init();
#if CFG_WATCHDOG_ENABLE
    wdt_enable(WDTO_250MS);
#endif
    sei();
    for (;;) {
        uint32_t now = timer_millis();
        if (gpio_stop_pressed()) {
            run_armed = false; /* Cancel even a RUN press that is still debouncing. */
            motor_stop(now);
        }
        adc_task(now);
        if ((uint32_t)(now - last_control) >= CFG_CONTROL_PERIOD_MS) {
            encoder_sample_t sample;
            bool run_event;
            bool stop_event;
            last_control = now;
            sample = encoder_snapshot();
            encoder_position = sample.position;
            if (sample.overflow || sample.invalid_transition) { window_bad = true; }
            if ((sample.delta > 0 && accumulated_counts > INT32_MAX - sample.delta) ||
                (sample.delta < 0 && accumulated_counts < INT32_MIN - sample.delta)) { window_bad = true; }
            else { accumulated_counts += sample.delta; }
            if (sample.overflow) { motor_raise_fault(MOTOR_FAULT_ENCODER); }
            run_event = debounce_update(&run_button, gpio_run_pressed(), now, CFG_DEBOUNCE_MS);
            stop_event = debounce_update(&stop_button, gpio_stop_pressed(), now, CFG_DEBOUNCE_MS);
#if CFG_ENCODER_COUNTS_PER_MEASURED_REV == 0
            if (stop_event) { count_origin = sample.position; } /* Zero the calibration display, not the ISR counter. */
#endif
            if (gpio_stop_pressed() || stop_event) {
                run_armed = false;
                motor_stop(now);
            } else {
                if (!run_button.stable && !run_button.candidate &&
                    (uint32_t)(now - run_button.since_ms) >= CFG_DEBOUNCE_MS) { run_armed = true; }
                if (run_event && run_armed) {
                    run_armed = false;
                    if (!adc_fresh(now)) { motor_raise_fault(MOTOR_FAULT_ADC); }
                    else { (void)motor_request((motor_direction_t)CFG_RUN_DIRECTION, now); }
                }
            }
            motor_task(now, control_adc_duty(adc_value()), adc_fresh(now), sample.moving);
            /* End the RPM interval at the same encoder snapshot that supplied
             * its last counts, even when the cooperative schedule drifts. */
            if ((uint32_t)(now - last_rpm) >= CFG_RPM_SAMPLE_MS) {
                rpm_valid = !window_bad && control_rpm(accumulated_counts, CFG_ENCODER_COUNTS_PER_MEASURED_REV, (uint32_t)(now - last_rpm), &rpm);
                displayed_encoder_bad = window_bad ? 1U : 0U;
                accumulated_counts = 0;
                window_bad = false;
                last_rpm = now;
            }
        }
        if ((uint32_t)(now - last_lcd) >= CFG_LCD_PERIOD_MS) {
            last_lcd = now;
            render(rpm, rpm_valid, control_adc_duty(adc_value()), displayed_encoder_bad, encoder_position - count_origin);
        }
        wdt_reset();
    }
}
