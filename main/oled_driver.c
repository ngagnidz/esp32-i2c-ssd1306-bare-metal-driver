#include <stdio.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#define SDA_PIN 21
#define SCL_PIN 22

void i2c_start()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SCL_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(SDA_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(SDA_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 0);
}

void i2c_stop()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SDA_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(SDA_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 0);
}

void write_bit(int bit)
{
    gpio_set_level(SCL_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(SDA_PIN, bit);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 0);
}

void i2c_write_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        write_bit(byte >> i & 1);
    }
}

int i2c_ack()
{
    gpio_set_direction(SDA_PIN, GPIO_MODE_INPUT);
    esp_rom_delay_us(5);
    gpio_set_level(SCL_PIN, 1);
    esp_rom_delay_us(5);
    int response = gpio_get_level(SDA_PIN); // Read
    gpio_set_level(SCL_PIN, 0);
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    return response;
}

void ssd1306_send_command(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(0x3C << 1 | 0);
    if (i2c_ack() == 1)
    {
        printf("Address Error");
        i2c_stop();
        return;
    }
    i2c_write_byte(0x00); // Next byte is a command
    if (i2c_ack() == 1)
    {
        printf("Control byte error");
        i2c_stop();
        return;
    }
    i2c_write_byte(cmd);
    if (i2c_ack() == 1)
    {
        printf("CMD byte error");
        i2c_stop();
        return;
    }
    i2c_stop();
}

void ssd1306_send_data(uint8_t data)
{
    i2c_start();
    i2c_write_byte(0x3C << 1 | 0);
    if (i2c_ack() == 1)
    {
        printf("Address Error");
        i2c_stop();
        return;
    }
    i2c_write_byte(0b01000000);
    if (i2c_ack() == 1)
    {
        printf("Control byte error");
        i2c_stop();
        return;
    }
    i2c_write_byte(data);
    if (i2c_ack() == 1)
    {
        printf("CMD byte error");
        i2c_stop();
        return;
    }
    i2c_stop();
}

// Initialization sequence specified by the datasheet Section 3 Figure 2.
void ssd1306_init()
{
    ssd1306_send_command(0xAE); // Display OFF
    ssd1306_send_command(0xA8); // Set MUX Ratio
    ssd1306_send_command(0x3F);
    ssd1306_send_command(0xD3); // Set Display Offset
    ssd1306_send_command(0x00);
    ssd1306_send_command(0x40); // Set Display Start Line
    ssd1306_send_command(0xA1); // Set Segment re-map (column 127 = SEG0)
    ssd1306_send_command(0xC8); // Set COM Output Scan Direction (remapped)
    ssd1306_send_command(0xDA); // Set COM Pins hardware configuration
    ssd1306_send_command(0x12);
    ssd1306_send_command(0xD5); // Set Osc Frequency
    ssd1306_send_command(0x80);
    ssd1306_send_command(0x81); // Set Contrast
    ssd1306_send_command(0x7F);
    ssd1306_send_command(0xA4); // Resume to RAM content display
    ssd1306_send_command(0xA6); // Set Normal Display
    ssd1306_send_command(0x8D); // Enable charge pump
    ssd1306_send_command(0x14);
    ssd1306_send_command(0xAF); // Display ON
}

uint8_t buffer[8][128];

void ssd1306_flush()
{
    // Set horizontal addressing mode
    ssd1306_send_command(0x20);
    ssd1306_send_command(0x00);

    // Set column 0 to 127
    ssd1306_send_command(0x21);
    ssd1306_send_command(0x00);
    ssd1306_send_command(127);

    // Set page 0 to 7
    ssd1306_send_command(0x22);
    ssd1306_send_command(0x00);
    ssd1306_send_command(0x07);
    for (int i = 0; i <= 127; i++)
    {
        for (int j = 0; j <= 7; j++)
        {
            ssd1306_send_data(buffer[j][i]);
        }
    }
}

void ssd1306_draw_pixel(int x, int y)
{
    int page = y / 8;
    int row = y % 8;
    buffer[page][x] |= (1 << row);
}

void ssd1306_draw_clear()
{
    for (int i = 0; i <= 127; i++)
    {
        for (int j = 0; j <= 7; j++)
        {
            buffer[j][i] = 0x00;
        }
    }
}

void app_main(void)
{
    esp_rom_delay_us(100000); // 100ms
    ssd1306_init();
    ssd1306_draw_clear();
    ssd1306_flush();
    ssd1306_draw_pixel(5, 5);
    ssd1306_flush();
    printf("starting\n");
    printf("\n");
}