#include "adc.h"
#include "config.h"
#include <avr/io.h>
static uint16_t filtered;
static uint16_t accumulator;
static uint32_t last_start;
static uint32_t last_valid;
static bool valid;
static bool discard_first;
static bool pending;

#if F_CPU / 8UL >= 50000UL && F_CPU / 8UL <= 200000UL
#define ADC_DIV_BITS (_BV(ADPS1) | _BV(ADPS0))
#elif F_CPU / 16UL >= 50000UL && F_CPU / 16UL <= 200000UL
#define ADC_DIV_BITS _BV(ADPS2)
#elif F_CPU / 32UL >= 50000UL && F_CPU / 32UL <= 200000UL
#define ADC_DIV_BITS (_BV(ADPS2) | _BV(ADPS0))
#elif F_CPU / 64UL >= 50000UL && F_CPU / 64UL <= 200000UL
#define ADC_DIV_BITS (_BV(ADPS2) | _BV(ADPS1))
#elif F_CPU / 128UL >= 50000UL && F_CPU / 128UL <= 200000UL
#define ADC_DIV_BITS (_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0))
#else
#error "ADC clock must be in the 50..200 kHz 10-bit range"
#endif

void adc_init(void)
{
    ADMUX = _BV(REFS0); /* AVCC reference, ADC0, right adjusted. AREF capacitor only. */
    ADCSRA = (uint8_t)(_BV(ADEN) | ADC_DIV_BITS);
    filtered = 0;
    accumulator = 0;
    last_start = 0;
    last_valid = 0;
    valid = false;
    discard_first = true;
    pending = false;
}

void adc_task(uint32_t now)
{
    if (pending && (ADCSRA & _BV(ADSC)) == 0) {
        uint8_t low = ADCL; /* Required read order locks the high byte until ADCH read. */
        uint16_t raw = (uint16_t)low | ((uint16_t)ADCH << 8U);
        raw &= CFG_ADC_FULL_SCALE;
        pending = false;
        if (discard_first) { discard_first = false; }
        else {
            if (!valid) { accumulator = (uint16_t)(raw * 8U); }
            else { accumulator = (uint16_t)(accumulator - (accumulator >> 3U) + raw); }
            filtered = (uint16_t)(accumulator >> 3U);
            last_valid = now;
            valid = true;
        }
    }
    if (!pending && (uint32_t)(now - last_start) >= CFG_ADC_PERIOD_MS) {
        last_start = now;
        ADCSRA |= _BV(ADSC);
        pending = true;
    }
}

uint16_t adc_value(void) { return filtered; }
bool adc_fresh(uint32_t now) { return valid && (uint32_t)(now - last_valid) <= CFG_ADC_STALE_MS; }
