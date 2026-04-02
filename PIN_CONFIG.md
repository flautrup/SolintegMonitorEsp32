# Hardware Pin Configuration

ESP32-C3 board with GC9A01 round display and CST816S touch controller.

## GC9A01 Display (SPI)

| Signal   | GPIO | Notes                                  |
|----------|------|----------------------------------------|
| MOSI     | 7    |                                        |
| MISO     | 8    | Required to prevent spiAttachMISO crash |
| SCLK     | 6    |                                        |
| CS       | 10   |                                        |
| DC       | 2    |                                        |
| RST      | -1   | Not wired                              |
| BL       | -1   | No backlight pin (always on)           |

**SPI Settings:**
- SPI Host: `SPI2_HOST`
- Write frequency: 80 MHz
- Read frequency: 20 MHz
- 3-wire mode: enabled
- DMA: `SPI_DMA_CH_AUTO`
- Colour invert: `true` (required for GC9A01)
- Readable: `false`
- Bus shared: `false`

## CST816S Touch Controller (I2C)

| Signal   | GPIO | Notes              |
|----------|------|--------------------|
| SDA      | 4    |                    |
| SCL      | 5    |                    |
| RST      | 1    |                    |
| INT      | 0    | Touch interrupt    |

**I2C Settings:**
- I2C address: `0x15`
- I2C port: `I2C_NUM_0`
- Speed: 400 kHz (fast mode)
