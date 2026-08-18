# OLEDTest

OLEDTest is a bare-metal OLED demonstration project based on the **STM32F103C8**. It drives an **SSD1306 128×64 OLED** through a GPIO-based software I²C bus and repeatedly displays 30 monochrome 64×64 cat frames in the center of the screen to create an animation.

The project uses the STM32F10x Standard Peripheral Library and can be opened directly in Keil MDK with `OLEDTest.uvprojx`.

## Features

- Drives an SSD1306 128×64 monochrome OLED.
- Implements software I²C on PB6 and PB7 without using the STM32 hardware I²C peripheral.
- Uses a 1 KB framebuffer for drawing pixels, lines, rectangles, circles, text, and bitmaps.
- Repeatedly plays a 30-frame 64×64 cat animation.
- Uses SysTick for millisecond timing and inter-frame delays.
- Connects the OLED driver to the bus through a callback, allowing the software I²C implementation to be replaced later.

## Hardware and Wiring

| Function | OLED Pin | STM32F103C8 Pin | Description |
| --- | --- | --- | --- |
| Power | VCC | 3.3 V | Follow the supply requirements of the specific OLED module |
| Ground | GND | GND | The OLED and STM32 must share a common ground |
| I²C clock | SCL | PB6 | GPIO open-drain output |
| I²C data | SDA | PB7 | GPIO open-drain output with ACK input capability |

PB6 and PB7 are configured as 2 MHz open-drain outputs. I²C requires pull-up resistors. Many OLED modules already include them; otherwise, add external pull-ups to SCL and SDA.

The display address is defined as `0x78` in `my_lib/oled.h`. This is the 8-bit write address including the R/W bit and corresponds to the common 7-bit I²C address `0x3C`. If the module uses the 7-bit address `0x3D`, change the value to `0x7A` to match this driver's address format.

## Peripherals and On-Chip Resources

| Peripheral/Resource | Purpose | Configuration |
| --- | --- | --- |
| GPIOB | Software I²C bus | PB6=SCL, PB7=SDA, open-drain output |
| RCC | System and GPIO clocks | Enables GPIOB; system clock configured for 72 MHz |
| SysTick | Millisecond time base | Interrupts every 1 ms and increments `ulTicks` |
| Flash | Program and animation storage | 30 × 512-byte frames occupy about 15 KB |
| SRAM | OLED framebuffer and runtime data | OLED initialization allocates 1,025 bytes, including a 1,024-byte framebuffer |

The application keeps only the software I²C driver that it actually uses. The optional `my_lib/i2c.c/.h` and `my_lib/usart.c/.h` modules have been removed. The Standard Peripheral Library directory still contains ST's low-level I²C, USART, and other peripheral modules; their presence does not mean those peripherals are enabled by the application.

## How It Works

### 1. Startup and Timing

After reset, the startup code calls `SystemInit()` to configure the system clock and then transfers control to `main()`. For the STM32F103C8 target, the clock configuration uses an external 8 MHz crystal and the PLL to produce a 72 MHz system clock.

The first call to `Delay()` invokes `Delay_Init()`, which configures SysTick from HCLK to generate one interrupt every millisecond. `SysTick_Handler()` increments the global `ulTicks` counter, while `Delay(ms)` implements a blocking millisecond delay by waiting for the counter to reach the target value.

### 2. Software I²C

`My_SoftwareI2C_Init()` assigns PB6 and PB7 to an `SI2C_TypeDef` instance. `My_SI2C_Init()` then enables the GPIO clocks and configures both pins as open-drain outputs.

For each transfer, `My_SI2C_SendBytes()` generates the following I²C sequence:

1. Start condition.
2. Slave address and write bit.
3. Data bytes with an ACK check after each byte.
4. Stop condition.

The OLED driver does not depend directly on the software I²C implementation. Instead, it calls the `i2c_write_bytes()` function registered through `OLED_InitTypeDef.i2c_write_cb`. A future hardware I²C implementation can therefore replace the callback without changing the drawing layer.

### 3. OLED Framebuffer and Refresh

The SSD1306 contains 128×64 pixels. The driver organizes the framebuffer into eight pages, with each page representing eight vertical pixels:

```text
128 columns × 8 pages = 1,024 bytes
Framebuffer index = x + (y / 8) × 128
Pixel bit         = y % 8
```

Drawing functions first update the framebuffer in STM32 RAM and do not immediately access the bus. When `OLED_SendBuffer()` is called, the driver selects horizontal addressing mode, sets the column range to 0–127 and the page range to 0–7, and then sends the complete 1,024-byte framebuffer to the display.

The control byte is `0x00` for commands and `0x40` for display data. OLED initialization also configures the scan direction, contrast, charge pump, and normal display mode.

### 4. Animation Playback

`cat_frames` in `my_lib/cat_frames.c` stores 30 bitmap frames. A 64×64 monochrome frame requires `64 × 64 / 8 = 512` bytes. The main loop performs the following sequence:

```text
Clear the RAM framebuffer
    ↓
Set the cursor to (32, 0) to center a 64-pixel-wide bitmap
    ↓
Draw one 64×64 frame into the framebuffer
    ↓
Send the complete 1,024-byte framebuffer to the OLED
    ↓
Wait 10 ms and continue with the next frame
    ↓
Restart after all 30 frames have been displayed
```

The actual frame rate also depends on the software I²C transfer time, so it is not exactly `1000 / 10 = 100 FPS`.

## Project Structure

```text
OLEDTest/
├─ user/
│  ├─ main.c                  # Application entry point, initialization, and playback loop
│  ├─ system_stm32f10x.c     # System clock configuration
│  └─ stm32f10x_it.c         # SysTick interrupt handler
├─ my_lib/
│  ├─ oled.c/.h               # SSD1306 initialization, drawing, framebuffer, and refresh
│  ├─ si2c.c/.h               # Software I²C driver used by the application
│  ├─ cat_frames.c            # Bitmap data for the 30 cat animation frames
│  ├─ cat_frames.h            # Animation dimensions, frame count, and data declaration
│  ├─ delay.c/.h              # SysTick delay and millisecond counter
│  ├─ oled_font.h             # Font data type definitions
│  ├─ oled_default_font.h     # Default font data
│  └─ font/                   # Font conversion tools and additional fonts
├─ std_periph_driver/         # STM32F10x Standard Peripheral Library
├─ startup/                   # Cortex-M3 startup file and interrupt vector table
├─ OLEDTest.uvprojx           # Keil MDK project file
└─ ARCHITECTURE.md            # Detailed architecture and data-flow notes
```

## Building and Flashing

1. Install Keil MDK and the STM32F1 Device Family Pack that supports the STM32F103C8.
2. Open `OLEDTest.uvprojx` in Keil.
3. Confirm that the selected device is `STM32F103C8`, then build the project.
4. Connect an ST-Link or another compatible debugger to the board.
5. Flash the firmware and reset the MCU. The OLED should repeatedly play the cat animation.

HEX generation is enabled. Build outputs are written to `Objects/`, and compiler listings are written to `Listings/`. Both directories are ignored by `.gitignore`.

## Customization

### Changing the OLED Pins

Edit the GPIO ports and pins in `My_SoftwareI2C_Init()` in `user/main.c`. If a different GPIO port is used, note that `si2c.c` currently enables clocks only for GPIOA through GPIOD, so its port-selection logic may also need to be extended.

### Changing the OLED Address

Change `OLED_SLAVE_ADDR` in `my_lib/oled.h`. This project uses a left-aligned 8-bit address that includes the R/W bit.

### Replacing the Animation

Convert the new images into monochrome bitmap data packed horizontally in groups of eight pixels. Replace the `cat_frames` array in `my_lib/cat_frames.c`, then update the frame count and dimensions in `my_lib/cat_frames.h`. The application calculates horizontal centering from the configured width. Each 64×64 frame occupies 512 bytes.

### Switching to Hardware I²C

To use hardware I²C, add or implement the required peripheral initialization and transfer functions, then register an adapter function through `i2c_write_cb`. The OLED drawing layer does not need to change.

## Troubleshooting

### The OLED Does Not Turn On

- Check VCC, GND, SCL, and SDA, and make sure the OLED and MCU share a common ground.
- Confirm that SCL and SDA have pull-up resistors.
- Confirm that the controller is an SSD1306 rather than an SH1106, which requires different initialization.
- Confirm that the module's 7-bit address is `0x3C`; update the driver address if it is `0x3D`.
- Confirm that the external crystal frequency matches the configuration in `system_stm32f10x.c`.

### The Image Orientation Is Incorrect

OLED initialization uses `0xA1` for segment remapping and `0xC8` for reversed COM scanning. Depending on the module orientation, try the corresponding `0xA0` and `0xC0` settings.

### The Animation Flickers or Runs at the Wrong Speed

Every frame refreshes the complete 1 KB framebuffer over software I²C. Adjust `Delay(10)` to change the inter-frame delay. For a higher refresh rate, use hardware I²C, increase the bus speed, or refresh only the regions that changed.

## Further Reading

See [ARCHITECTURE.md](ARCHITECTURE.md) for more detailed module relationships, peripheral resource diagrams, and per-frame data flow.
