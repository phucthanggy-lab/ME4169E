#ifndef ADC_H
#define ADC_H
#include <stdbool.h>
#include <stdint.h>
void adc_init(void);
void adc_task(uint32_t now);
uint16_t adc_value(void);
bool adc_fresh(uint32_t now);
#endif
