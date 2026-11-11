#include "config.h"
#include "lcd.h"
#include <avr/io.h>
#include <util/delay.h>

static void nibble(uint8_t value)
{
    PORTD = (uint8_t)((PORTD & 0x0FU) | ((value & 0x0FU) << 4U));
    _delay_us(1);
    PORTD |= _BV(PD1);
    _delay_us(1);
    PORTD &= (uint8_t)~_BV(PD1);
    _delay_us(1);
}

static void send(uint8_t value, uint8_t data)
{
    if (data) { PORTD |= _BV(PD0); } else { PORTD &= (uint8_t)~_BV(PD0); }
    nibble((uint8_t)(value >> 4U));
    nibble(value);
    _delay_us(CFG_LCD_COMMAND_US);
}

void lcd_init(void)
{
    PORTD &= (uint8_t)~0xF3U;
    DDRD |= 0xF3U; /* Keep PD2/PD3 encoder inputs. RW is physically grounded. */
    _delay_ms(CFG_LCD_POWERUP_MS);
    nibble(3U);
    _delay_ms(5);
    nibble(3U);
    _delay_us(150);
    nibble(3U);
    _delay_us(150);
    nibble(2U);
    _delay_us(150);
    send(0x28U, 0U); /* 4-bit, 2-line, 5x8 */
    send(0x08U, 0U);
    send(0x01U, 0U);
    _delay_us(CFG_LCD_CLEAR_US);
    send(0x06U, 0U);
    send(0x0CU, 0U);
}

void lcd_write_line(uint8_t row, const char *text)
{
    uint8_t index;
    uint8_t ended = 0U;
    if (row > 1U || text == 0) { return; }
    send(row == 0U ? 0x80U : 0xC0U, 0U);
    for (index = 0; index < 16U; index++) {
        char character = ' ';
        if (!ended) {
            if (*text == '\0') { ended = 1U; }
            else { character = *text++; }
        }
        send((uint8_t)character, 1U); /* Fixed width clears stale characters without clear/flicker. */
    }
}
