#ifndef LCD_H
#define LCD_H
#include <stdint.h>
void lcd_init(void);
void lcd_write_line(uint8_t row, const char *text);
#endif
