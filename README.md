# ESP32 SSD1306 Bare-Metal Driver

I2C driver for the SSD1306 OLED display on the ESP32, written in C without any HAL, framework abstractions, or libraries. GPIO is controlled by writing directly to memory-mapped hardware registers. I2C is bit-banged by hand.

---

## What it does

- Drives a 128×64 SSD1306 OLED over I2C
- Implements the full I2C protocol from scratch: START, STOP, clock, byte transmission, ACK
- Accesses GPIO through direct register writes - no `gpio_set_level()` or ESP-IDF HAL
- Maintains a 1024-byte framebuffer and flushes it to the display in one I2C transaction
- Renders text using an 8x8 bitmap font with automatic line wrapping

---

## Hardware

- ELEGOO ESP32 development board
- 0.96" SSD1306 OLED (128×64, I2C address `0x3C`)
- SDA - GPIO 21
- SCL - GPIO 22
- 3.3V and GND from the ESP32 dev board

---

## Build and flash

```bash
idf.py build flash monitor   
```

---

## How it works

### Register-direct GPIO

Instead of calling `gpio_set_level()` or `gpio_set_direction()`, the driver writes directly to the ESP32's memory mapped GPIO registers:

Set pin high `GPIO_OUT_W1TS = (1 << pin)` 
Set pin low `GPIO_OUT_W1TC = (1 << pin)` 
Set as output `GPIO_ENABLE_W1TS = (1 << pin)` 
Set as input `GPIO_ENABLE_W1TC = (1 << pin)` 
Read pin `(GPIO_IN >> pin) & 1` 

W1TS (write 1 to set) and W1TC (write 1 to clear) are single-bit operations. Writing a 1 to bit N affects only pin N.

### Bit-banged I2C

I2C is implemented entirely in software. The driver manually changes SDA and SCL to create waverforms.

- **START:** SDA falls while SCL is high
- **Data bits:** SDA is set before SCL rise and slave reads on the rising edge
- **ACK:** SDA is released (set as input) so the slave can pull it low to acknowledge
- **STOP:** SDA rises while SCL is high

Timing is created using the Xtensa LX6 CPU's hardware cycle counter (`CCOUNT`) using inline assembly. At 160 MHz, each microsecond is 160 cycles.

### Framebuffer and single-transaction flush

The SSD1306 GDDRAM is organized as 8 pages x 128 columns. Each byte represents a 1x8 vertical slice of pixels. The bits map to individual pixel rows within that page.

The driver has a 1024-byte framebuffer in RAM. Drawing functions (`draw_pixel`, `draw_char`, `draw_string`) write into this buffer. When `ssd1306_flush()` is called, the entire buffer is sent to the display in one I2C transaction:

```
START → address → control byte (0x40) → [1024 data bytes] → STOP
```

The display is put into horizontal addressing mode (`0x20, 0x00`), so it auto increments through every column and page without the driver needing to reset the cursor. Initial approach called `ssd1306_send_data()` once per byte. It opened and closed a full I2C transaction (START, address, ACK, control byte, ACK, data, ACK, STOP) for each byte. The single transaction flush reduces that to one START and one STOP for the entire frame buffer.

### I2C open-drain topology

I2C is an open-drain bus. A master and slave do not ever actively drives the line high. High is the default state held by pull-up resistors. Devices can only pull the line low. This means a master releases SDA by switching the pin to input mode, allowing the pull-up to bring it high and letting the slave drive if needed.

---