#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

void i2c_bus_init(void);
esp_err_t i2c_write_bytes(uint8_t addr, const uint8_t *data, size_t len);
esp_err_t i2c_read_bytes(uint8_t addr, uint8_t *data, size_t len);
esp_err_t i2c_write_read_bytes(uint8_t addr, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen);
esp_err_t tca_select(uint8_t ch);
void tca_off(void);
void recover_i2c_bus(void);

#endif
