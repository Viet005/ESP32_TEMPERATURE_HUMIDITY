#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "app_types.h"

void init_dht_pin(void);
void sensors_probe_all(void);
void read_all_sensors(void);

#endif
