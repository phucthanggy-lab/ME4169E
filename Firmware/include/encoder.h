#ifndef ENCODER_H
#define ENCODER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t position;
    int32_t delta;
    uint16_t invalid;
    bool overflow;
    bool moving;
    bool invalid_transition;
} encoder_sample_t;
void encoder_init(void);
void encoder_process_state(uint8_t state);
encoder_sample_t encoder_snapshot(void);
#endif
