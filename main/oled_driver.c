#include <stdio.h>
#include "font8x8_basic.h"

#define SDA_PIN 21
#define SCL_PIN 22

/*
 * Bare-metal GPIO access on ESP32.
 * Instead of using gpio_set_level() / gpio_set_direction() from the HAL,
 * writes directly to the memory-mapped GPIO registers.
 *
 * W1TS = Write 1 To Set sets the bit high
 * W1TC = Write 1 To Clear sets the  bit low
 *
 *   gpio_set_level(SCL_PIN, 1)           GPIO_OUT_W1TS = (1 << SCL_PIN)
 *   gpio_set_level(SCL_PIN, 0)           GPIO_OUT_W1TC = (1 << SCL_PIN)
 *   gpio_set_direction(SDA_PIN, OUTPUT)  GPIO_ENABLE_W1TS = (1 << SDA_PIN)
 *   gpio_set_direction(SDA_PIN, INPUT)   GPIO_ENABLE_W1TC = (1 << SDA_PIN)
 *   gpio_get_level(SDA_PIN)              (GPIO_IN >> SDA_PIN) & 1
 */

// volatile tells the compiler to not re order or cache any read/write.
// GPIO0–GPIO31 are covered by these 32-bit registers.
#define GPIO_OUT_W1TS (*(volatile uint32_t *)0x3FF44008)
#define GPIO_OUT_W1TC (*(volatile uint32_t *)0x3FF4400C)
#define GPIO_ENABLE_W1TS (*(volatile uint32_t *)0x3FF44024)
#define GPIO_ENABLE_W1TC (*(volatile uint32_t *)0x3FF44028)
#define GPIO_IN (*(volatile uint32_t *)0x3FF4403C)

// CCOUNT is a hardware cycle counter built into the Xtensa LX6 CPU.
// Increments once per clock cycle — at 160 MHz = 160 ticks per microsecond.
static inline uint32_t get_ccount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=r"(ccount));
    return ccount;
}

// Cycle accurate delay using the CPU cycle counter.
// Snapshot CCOUNT at the start and spin until enough cycles have elapsed.
// Subtraction wraprs around.
static inline void delay_us(uint32_t us)
{
    uint32_t start = get_ccount();
    while ((get_ccount() - start) < us * 160)
        ;
}

// I2C START: SDA goes low while SCL is high.
void i2c_start()
{
    GPIO_ENABLE_W1TS = (1 << SDA_PIN); // SDA as output
    GPIO_ENABLE_W1TS = (1 << SCL_PIN); // SCL as output
    GPIO_OUT_W1TS = (1 << SCL_PIN);    // set SCL to 1
    delay_us(5);
    GPIO_OUT_W1TS = (1 << SDA_PIN); // set SDA to 1
    delay_us(5);
    GPIO_OUT_W1TC = (1 << SDA_PIN); // set SDA to 0
    delay_us(5);
    GPIO_OUT_W1TC = (1 << SCL_PIN); // set SCL to 0
}

// I2C STOP condition: SDA goes high while SCL is high.
void i2c_stop()
{
    GPIO_OUT_W1TS = (1 << SCL_PIN); // set SCL to 1
    delay_us(5);
    GPIO_OUT_W1TC = (1 << SDA_PIN); // set SDA to 0
    delay_us(5);
    GPIO_OUT_W1TS = (1 << SDA_PIN); // set SDA to 1
    delay_us(5);
    GPIO_OUT_W1TC = (1 << SCL_PIN); // set SCL to 0
}

// Data must be stable before SCL rises. Setup time is done by delay_us.
void write_bit(int bit)
{
    GPIO_OUT_W1TC = (1 << SCL_PIN); // set SCL to 0
    delay_us(5);
    if (bit == 1)
    {
        GPIO_OUT_W1TS = (1 << SDA_PIN); // set SDA to 1
    }
    else
    {
        GPIO_OUT_W1TC = (1 << SDA_PIN); // set SDA to 0
    }
    delay_us(5);
    GPIO_OUT_W1TS = (1 << SCL_PIN); // set SCL to 1
    delay_us(5);
    GPIO_OUT_W1TC = (1 << SCL_PIN); // set SCL to 0
}

// Send one byte MSB-first
void i2c_write_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        write_bit(byte >> i & 1);
    }
}

// Read the ACK/NACK bit from the slave.
// Returns 0 = ACK success, 1 = NACK error
int i2c_ack()
{
    GPIO_ENABLE_W1TC = (1 << SDA_PIN); // SDA as input
    delay_us(5);
    GPIO_OUT_W1TS = (1 << SCL_PIN); // set SCL to 1
    delay_us(5);
    int response = GPIO_IN >> SDA_PIN & 1;
    GPIO_ENABLE_W1TS = (1 << SDA_PIN);
    return response;
}

// Send a command byte to the SSD1306.
// I2C packet: START 0x78 addr ACK 0x00 control ACK cmd ACK STOP
// Control byte 0x00 means the next byte is a command (0x40 for data).
void ssd1306_send_command(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(0x3C << 1 | 0); // 7 bit address 0x3C and write bit = 0
    if (i2c_ack() == 1)
    {
        printf("Address error \n");
        return;
    }

    i2c_write_byte(0x00); // control byte
    if (i2c_ack() == 1)
    {
        printf("Control byte error \n");
        return;
    }
    delay_us(5);

    i2c_write_byte(cmd);
    if (i2c_ack() == 1)
    {
        printf("Command write error \n");
        return;
    }
    i2c_stop();
}

// Send a data byte to the SSD1306 GDDRAM (display memory).
// Same as send_command but control byte is 0x40
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
    delay_us(5);

    i2c_write_byte(data);
    if (i2c_ack() == 1)
    {
        printf("Command write error \n");
        return;
    }
    i2c_stop();
}

// Initialize the SSD1306 with the startup sequence from the datasheet
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

// 128 columns x 8 pages = 1024 bytes.
// Each byte is one 1x8 vertical column of pixels.
// Each bit in the byte is one pixel row within that page.
uint8_t framebuffer[1024] = {0};

// Set a single pixel in the framebuffer.
// x = column (0–127), y = row (0–63).
// page = y / 8 which of the 8 horizontal pages
// bit  = y % 8 which bit within that page byte
void ssd1306_draw_pixel(int x, int y)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    int page = y / 8;
    int d = page * 128 + x;
    framebuffer[d] |= 1 << y % 8;
}

// Push whole framebuffer to the display over I2C.
// We use horizontal addressing mode so the display auto increments through all columns and pages.
// The full 1024 bytes are sent in one I2C transaction.
void ssd1306_flush()
{
    // Horizontal addressing mode — auto-increments column, then page
    ssd1306_send_command(0x20);
    ssd1306_send_command(0x00);

    // Column range: 0 to 127
    ssd1306_send_command(0x21);
    ssd1306_send_command(0x00);
    ssd1306_send_command(127);

    // Page range: 0 to 7
    ssd1306_send_command(0x22);
    ssd1306_send_command(0x00);
    ssd1306_send_command(7);

    // Send all 1024 framebuffer bytes in one transaction
    i2c_start();
    i2c_write_byte(0x3C << 1 | 0);
    if (i2c_ack() == 1)
    {
        printf("Address error \n");
        return;
    }

    i2c_write_byte(0x40); // control byte: all following bytes are data
    if (i2c_ack() == 1)
    {
        printf("Control byte error\n");
        return;
    }

    for (int i = 0; i < 1024; i++)
    {
        i2c_write_byte(framebuffer[i]);
        if (i2c_ack() == 1)
        {
            printf("Write Error \n");
            return;
        }
    }
    i2c_stop();
}

// Zero out the framebuffer
void ssd1306_clear()
{
    for (int i = 0; i < 1024; i++)
    {
        framebuffer[i] = 0;
    }
}

// Draw a single ASCII character using the 8x8 bitmap font.
// Each character is 8 rows tall, each row is a byte where each bit is a pixel.
void ssd1306_draw_char(int c, int x, int y)
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

// Draw a null-terminated string starting at (x, y).
// Wraps to the next line (y += 8).
void ssd1306_draw_string(char *str, int x, int y)
{
    while (*str != '\0')
    {
        if (y >= 64)
            return;
        ssd1306_draw_char(*str, x, y);
        if (x + 8 >= 128)
        {
            y += 8;
            x = 0;
        }
        else
        {
            x += 8;
        }

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
    ssd1306_draw_string("SSD1306 Bare Metal Driver", 0, 0);
    ssd1306_flush();
}