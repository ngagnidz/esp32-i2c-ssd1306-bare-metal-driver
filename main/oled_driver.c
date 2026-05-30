#include <stdio.h>
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define SDA_PIN 21
#define SCL_PIN 22

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

void write_bit(int bit)
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SDA_PIN, bit);
    ets_delay_us(5);
    gpio_set_level(SCL_PIN, 1);
    ets_delay_us(5);
    gpio_set_level(SCL_PIN, 0);
}

void i2c_write_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        write_bit((byte >> i) & 1); // shift by i and then and operation with 1 for bit extraction
    }
}

int i2c_ack()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_INPUT);
    ets_delay_us(5); // Allow the slave to pull the line and then read
    gpio_set_level(SCL_PIN, 1);
    int slave_signal = gpio_get_level(SDA_PIN);
    gpio_set_level(SCL_PIN, 0);
    return slave_signal;
}

void ssd1306_send_command(uint8_t cmd)
{
    // Initiate the communication
    i2c_start();

    // Call the address of the display. Set the mode to WRITE
    i2c_write_byte(0x3C << 1 | 0);

    if (i2c_ack() != 0)
    {
        printf("NACK on address. Exiting...");
        i2c_stop();
        return;
    }

    /*Following byte is a command.
    Alternative: Following byte is data that you need to write into GDDRAM*/
    i2c_write_byte(0x00);

    if (i2c_ack() != 0)
    {
        printf("NACK on control byte. Exiting...");
        i2c_stop();
        return;
    }

    i2c_write_byte(cmd);

    if (i2c_ack() != 0)
    {
        printf("NACK on command. Exiting...");
        i2c_stop();
    }
    i2c_stop();
}

void app_main(void)
{
    ssd1306_send_command(0xAE);
    ets_delay_us(50000);
    ssd1306_send_command(0xAF);
}