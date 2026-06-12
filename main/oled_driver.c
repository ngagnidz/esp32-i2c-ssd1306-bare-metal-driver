#include <stdio.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "font8x8_basic.h"

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
    gpio_set_level(SCL_PIN, 1);
    esp_rom_delay_us(5);

    gpio_set_level(SDA_PIN, 0);

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
    int response = gpio_get_level(SDA_PIN);
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    return response;
}

void ssd1306_send_command(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(0x3C << 1 | 0);
    if (i2c_ack() == 1)
    {
        printf("Address error \n");
        return;
    }

    i2c_write_byte(0x00);
    if (i2c_ack() == 1)
    {
        printf("Control byte error \n");
        return;
    }
    esp_rom_delay_us(5);

    i2c_write_byte(cmd);
    if (i2c_ack() == 1)
    {
        printf("Command write error \n");
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
        printf("Address error \n");
        return;
    }

    i2c_write_byte(0x40);
    if (i2c_ack() == 1)
    {
        printf("Control byte error \n");
        return;
    }
    esp_rom_delay_us(5);

    i2c_write_byte(data);
    if (i2c_ack() == 1)
    {
        printf("Command write error \n");
        return;
    }
    i2c_stop();
}

// 0xAE         display off
// 0xA8, 0x3F   MUX ratio = 64
// 0xD3, 0x00   display offset = 0
// 0x40         start line = 0
// 0xA1         segment remap
// 0xC8         COM scan direction (remapped)
// 0xDA, 0x12   COM pins config
// 0xD5, 0x80   osc frequency
// 0x81, 0x7F   contrast
// 0xA4         resume from RAM
// 0xA6         normal display
// 0x8D, 0x14   charge pump on
// 0xAF         display on
void ssd1306_init()
{
    ssd1306_send_command(0xAE); // display off
    ssd1306_send_command(0xA8); // MUX ratio
    ssd1306_send_command(0x3F); //   = 64
    ssd1306_send_command(0xD3); // display offset
    ssd1306_send_command(0x00); //   = 0
    ssd1306_send_command(0x40); // start line = 0
    ssd1306_send_command(0xA1); // segment remap
    ssd1306_send_command(0xC8); // COM scan direction (remapped)
    ssd1306_send_command(0xDA); // COM pins config
    ssd1306_send_command(0x12);
    ssd1306_send_command(0xD5); // osc frequency
    ssd1306_send_command(0x80);
    ssd1306_send_command(0x81); // contrast
    ssd1306_send_command(0x7F);
    ssd1306_send_command(0xA4); // resume from RAM
    ssd1306_send_command(0xA6); // normal display
    ssd1306_send_command(0x8D); // charge pump
    ssd1306_send_command(0x14); //   on
    ssd1306_send_command(0xAF); // display on
}

uint8_t framebuffer[1024] = {0}; // We have 1024 columns 8 pages * 128 columns. Each column has 8 rows. 1 column takes in 1 byte.
// Each bit in a byte is one row.

void ssd1306_draw_pixel(int x, int y)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    int page = y / 8;
    int d = page * 128 + x;
    framebuffer[d] |= 1 << y % 8;
}

void ssd1306_flush()
{
    // Horizontal addressing mode
    ssd1306_send_command(0x20);
    ssd1306_send_command(0x00);

    // Set column range
    ssd1306_send_command(0x21);
    ssd1306_send_command(0x00);
    ssd1306_send_command(127);

    // Set page range
    ssd1306_send_command(0x22);
    ssd1306_send_command(0x00);
    ssd1306_send_command(7);

    for (int i = 0; i < 1024; i++)
    {
        ssd1306_send_data(framebuffer[i]);
    }
}

void ssd1306_clear()
{
    for (int i = 0; i < 1024; i++)
    {
        framebuffer[i] = 0;
    }
}

void ssd1306_draw_char(char c, int x, int y)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            if (font8x8_basic[(int)c][row] >> col & 1)
            {
                ssd1306_draw_pixel(x + col, y + row);
            }
        }
    }
}

void ssd1306_draw_string(char *str, int x, int y)
{
    while (*str != '\0')
    {
        ssd1306_draw_char(*str, x, y);
        x += 8;
        str++;
    }
}

void app_main(void)
{
    ssd1306_init();
    ssd1306_clear();
    ssd1306_draw_string("Hello World", 0, 0);
    ssd1306_flush();
    ssd1306_clear();
    ssd1306_draw_string("Bye World", 0, 0);
    ssd1306_flush();
}