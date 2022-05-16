/*
 * Description: GPIO driver for AAUSat camera payload
 * Dato: 		2022-04-06
 * Author:		Stephan
 */

#pragma once

int gpio_init(void);
int gpio_toggle(int pin);
int gpio_set(int pin, int lvl);
