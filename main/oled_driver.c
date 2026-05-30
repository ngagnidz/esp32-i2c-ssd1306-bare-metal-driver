#include <stdio.h>
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define SDA_PIN 21
#define SCL_PIN 22

void app_main(void)
{
}

void i2c_start()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SCL_PIN, 1);
    gpio_set_level(SDA_PIN, 1);
    ets_delay_us(5);
    gpio_set_level(SDA_PIN, 0);
    ets_delay_us(5);
    gpio_set_level(SCL_PIN, 0);
}

void i2c_stop()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SCL_PIN, 1);
    gpio_set_level(SDA_PIN, 0);
    ets_delay_us(5);
    gpio_set_level(SDA_PIN, 1);
    ets_delay_us(5);
    gpio_set_level(SCL_PIN, 1);
}
