#ifndef CONFIG_H
#define CONFIG_H

/* User-selected ATmega8 / L298 / 12 V / 8 MHz board configuration.
 * Encoder calibration and physical reset tests are not yet available. */
#define FW_PROFILE_DEVELOPMENT 1
#define FW_PROFILE_RELEASE 2
#define FW_PROFILE_TEST 3
#ifndef FW_PROFILE
#define FW_PROFILE FW_PROFILE_RELEASE
#endif
#define CFG_REPORTED_MCU "ATmega8 project target; exact ordering suffix not recorded"
#define CFG_BUILD_MCU "atmega8"
#ifndef CFG_MCU_CONFIRMED
#define CFG_MCU_CONFIRMED 1
#endif
#define CFG_CLOCK_SOURCE "8 MHz selected by user; oscillator source/fuses not read"
#define CFG_FUSE_CONFIGURATION "UNCONFIRMED - DO NOT WRITE"
#define CFG_DRIVER_MODEL "L298 bridge A: PB0=IN1, PB2=IN2, PB1=ENA"
#define CFG_MOTOR_MODEL "DC 385 Hall encoder, seller code kde3852"
#define CFG_MOTOR_SUPPLY_MV 12000UL
#define CFG_LCD_CONTROLLER "ASSUMPTION: HD44780 compatible, parallel 4-bit"
#ifndef CFG_HW_F_CPU_HZ
#define CFG_HW_F_CPU_HZ 8000000UL
#endif
#ifndef CFG_CLOCK_CONFIRMED
#define CFG_CLOCK_CONFIRMED 1
#endif
#ifndef CFG_DRIVER_CONFIRMED
#define CFG_DRIVER_CONFIRMED 1
#endif
#ifndef CFG_RESET_SAFETY_CONFIRMED
/* Physical test status only. Fit ENA-to-GND 10k; remove module ENA jumper.
 * A compiler cannot confirm the external reset-disable circuit. */
#define CFG_RESET_SAFETY_CONFIRMED 0
#endif
#ifndef CFG_MOTOR_OUTPUTS_ENABLE
#if FW_PROFILE == FW_PROFILE_RELEASE
#define CFG_MOTOR_OUTPUTS_ENABLE 1
#else
#define CFG_MOTOR_OUTPUTS_ENABLE 0
#endif
#endif
#ifndef CFG_DIRECTION_ACTIVE_LEVEL
#define CFG_DIRECTION_ACTIVE_LEVEL 1
#endif
#ifndef CFG_PWM_ACTIVE_LEVEL
#define CFG_PWM_ACTIVE_LEVEL 1
#endif
#ifndef CFG_PWM_TARGET_HZ
#define CFG_PWM_TARGET_HZ 10000UL
#endif
#ifndef CFG_MOTOR_DUTY_LIMIT_PERMILLE
#define CFG_MOTOR_DUTY_LIMIT_PERMILLE 1000U
#endif
#ifndef CFG_ENCODER_PPR
#define CFG_ENCODER_PPR 0UL
#endif
/* Counts per measured shaft revolution in the selected mode, after gearing.
 * Quadrature counts x4 edges; Hall mode counts rising edges x1. Calibrate each separately. */
#ifndef CFG_ENCODER_COUNTS_PER_MEASURED_REV
#define CFG_ENCODER_COUNTS_PER_MEASURED_REV 0UL
#endif
#define CFG_ENCODER_QUADRATURE_X4 4U
#define CFG_ENCODER_HALL_RISING 1U
#ifndef CFG_ENCODER_MODE
/* A/B follows the schematic; Hall-only is an explicit alternative build. */
#define CFG_ENCODER_MODE CFG_ENCODER_QUADRATURE_X4
#endif
#ifndef CFG_ENCODER_REVERSED
#define CFG_ENCODER_REVERSED 0
#endif
#if CFG_ENCODER_MODE != CFG_ENCODER_QUADRATURE_X4 && CFG_ENCODER_MODE != CFG_ENCODER_HALL_RISING
#error "Select quadrature x4 (4) or single-channel Hall rising-edge (1)"
#endif
#if CFG_ENCODER_REVERSED != 0 && CFG_ENCODER_REVERSED != 1
#error "Encoder reversed must be 0 or 1"
#endif
#if CFG_ENCODER_MODE == CFG_ENCODER_HALL_RISING && CFG_ENCODER_REVERSED
#error "Single-channel Hall cannot measure rotation direction"
#endif
#define CFG_ENCODER_DECODE_MULTIPLIER CFG_ENCODER_MODE
#define CFG_ENCODER_LOGIC_MV 0U
#define CFG_GEAR_RATIO_NUMERATOR 0UL
#define CFG_GEAR_RATIO_DENOMINATOR 0UL
#ifndef CFG_REVERSAL_SAFETY_CONFIRMED
#define CFG_REVERSAL_SAFETY_CONFIRMED 0
#endif
#ifndef CFG_REVERSAL_DEADTIME_MS
#define CFG_REVERSAL_DEADTIME_MS 0UL
#endif
#ifndef CFG_REVERSAL_QUIET_MS
#define CFG_REVERSAL_QUIET_MS 0UL
#endif
#ifndef CFG_REVERSAL_TIMEOUT_MS
#define CFG_REVERSAL_TIMEOUT_MS 0UL
#endif
/* RUN selects L298 forward; STOP does not request reversal. */
#ifndef CFG_RUN_DIRECTION
#define CFG_RUN_DIRECTION 1U
#endif

/* ASSUMPTION: software-only development clock, NOT a fuse/board assertion. */
#ifndef CFG_DEV_F_CPU_HZ
#define CFG_DEV_F_CPU_HZ 1000000UL
#endif
#if FW_PROFILE == FW_PROFILE_DEVELOPMENT
#if CFG_MOTOR_OUTPUTS_ENABLE
#error "Development profile must lock motor outputs"
#endif
#define F_CPU CFG_DEV_F_CPU_HZ
#elif FW_PROFILE == FW_PROFILE_RELEASE
#if !CFG_MCU_CONFIRMED
#error "Release build requires the actual MCU marking/BOM confirmation"
#endif
#if !CFG_CLOCK_CONFIRMED || CFG_HW_F_CPU_HZ == 0
#error "Release build requires a confirmed physical clock"
#endif
#define F_CPU CFG_HW_F_CPU_HZ
#elif FW_PROFILE == FW_PROFILE_TEST
#ifndef FW_TESTING
#error "Test profile is only for the synthetic verification harness"
#endif
#define F_CPU CFG_DEV_F_CPU_HZ
#else
#error "Unknown build profile"
#endif
#if F_CPU < 1000000UL || F_CPU > 16000000UL
#error "This implementation supports CPU clocks from 1 to 16 MHz"
#endif
#if CFG_MOTOR_OUTPUTS_ENABLE
#if !CFG_MCU_CONFIRMED || !CFG_CLOCK_CONFIRMED || !CFG_DRIVER_CONFIRMED
#error "Motor output enable requires selected MCU, clock and driver configuration"
#endif
#if CFG_DIRECTION_ACTIVE_LEVEL < 0 || CFG_DIRECTION_ACTIVE_LEVEL > 1
#error "Confirm direction active level"
#endif
#if CFG_PWM_ACTIVE_LEVEL < 0 || CFG_PWM_ACTIVE_LEVEL > 1
#error "Confirm PWM active level"
#endif
#if CFG_PWM_TARGET_HZ == 0 || CFG_MOTOR_DUTY_LIMIT_PERMILLE == 0 || CFG_MOTOR_DUTY_LIMIT_PERMILLE > 1000
#error "Confirm PWM frequency and permitted nonzero duty limit"
#endif
#if CFG_RUN_DIRECTION != 1 && CFG_RUN_DIRECTION != 2
#error "Specify the RUN button direction policy"
#endif
#endif

/* Software policy choices, tunable after real clock/noise/response testing. */
#define CFG_CONTROL_PERIOD_MS 10UL
#define CFG_ADC_PERIOD_MS 10UL
#define CFG_ADC_STALE_MS 100UL
#define CFG_DEBOUNCE_MS 25UL
#define CFG_RPM_SAMPLE_MS 250UL
#define CFG_LCD_PERIOD_MS 200UL
#define CFG_MAIN_STALL_MS 100UL
#define CFG_DUTY_RAMP_STEP_PERMILLE 20U
#define CFG_WATCHDOG_ENABLE 1
#define CFG_ADC_FULL_SCALE 1023U
#define CFG_DUTY_SCALE 1000U
#define CFG_BUTTON_INTERNAL_PULLUPS 0
#define CFG_ENCODER_INTERNAL_PULLUPS 0
#define CFG_LCD_POWERUP_MS 50
#define CFG_LCD_COMMAND_US 60
#define CFG_LCD_CLEAR_US 2500

/* Timer2 CTC, exact nominal 1 ms at the compiled clock; OC2 stays disconnected.
 * Reject clocks with no exact integer divider instead of silently lying about time. */
#if F_CPU % 8000UL == 0 && F_CPU / 8000UL <= 256
#define CFG_TIMER2_DIVIDER 8UL
#define CFG_TIMER2_CS_BITS 2U
#elif F_CPU % 32000UL == 0 && F_CPU / 32000UL <= 256
#define CFG_TIMER2_DIVIDER 32UL
#define CFG_TIMER2_CS_BITS 3U
#elif F_CPU % 64000UL == 0 && F_CPU / 64000UL <= 256
#define CFG_TIMER2_DIVIDER 64UL
#define CFG_TIMER2_CS_BITS 4U
#elif F_CPU % 128000UL == 0 && F_CPU / 128000UL <= 256
#define CFG_TIMER2_DIVIDER 128UL
#define CFG_TIMER2_CS_BITS 5U
#else
#error "Clock has no supported exact 1ms Timer2 setting; configure a fractional scheduler first"
#endif
#define CFG_TIMER2_OCR ((F_CPU / CFG_TIMER2_DIVIDER / 1000UL) - 1UL)
#endif
