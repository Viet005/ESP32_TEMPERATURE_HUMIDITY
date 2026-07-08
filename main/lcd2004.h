#ifndef LCD2004_H
#define LCD2004_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(void);
void lcd_print20(uint8_t row, const char *text);
void lcd_apply_backlight(void);

#endif
