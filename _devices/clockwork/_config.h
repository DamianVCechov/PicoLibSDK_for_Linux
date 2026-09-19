// PicoLibSDK - Alternative SDK library for Raspberry Pico and RP2040
// Copyright (c) 2023 Miroslav Nemecek, Panda38@seznam.cz, hardyplotter2@gmail.com
// 	https://github.com/Panda381/PicoLibSDK
//	https://www.breatharian.eu/hw/picolibsdk/index_en.html
//	https://github.com/pajenicko/picopad
//	https://picopad.eu/en/
// License:
//	This source code is freely available for any purpose, including commercial.
//	It is possible to take and modify the code or parts of it, without restriction.

#ifndef USE_FRAMEBUF
#if USE_VIDEO
#define USE_FRAMEBUF	0
#else
#define USE_FRAMEBUF	1		// use default display frame buffer
#endif
#endif

// ============================================================================
//                            Picocalc Clockwork
// ============================================================================

// === Display

#if USE_CLOCKWORK

#ifndef DISP_ROT
#define DISP_ROT	0		// display rotation of LCD: 0 Portrait, 1 Landscape, 2 Inverted Portrait, 3 Inverted Landscape
#endif

#ifndef WIDTH
#if (DISP_ROT == 0) || (DISP_ROT == 2)
#define WIDTH		320		// display width
#else
#define WIDTH		320		// display width
#endif
#endif

#ifndef HEIGHT
#if (DISP_ROT == 0) || (DISP_ROT == 2)
#define HEIGHT		320		// display height
#else
#define HEIGHT		320		// display height
#endif
#endif

#ifndef COLBITS
#define COLBITS		16		// number of output color bits (4, 8, 15 or 16)
#endif

#define COLTYPE		u16		// type of color: u8, u16 or u32
#define FRAMETYPE	u16		// type of frame entry: u8 or u16
#define WIDTHLEN	WIDTH		// length of one line of one plane, in number of frame elements
#define FRAMESIZE 	(WIDTHLEN*HEIGHT) // frame size in number of colors
#define	DISP_STRIP_NUM	1		// number of back strips

#ifndef USE_ST7365P
#define USE_ST7365P		1		// use ST7365P TFT display (st7365p.c, st7365p.h)
#endif

#ifndef USE_DRAW
#define USE_DRAW	1	// use drawing to frame buffer (lib_draw.c, lib_draw.h)
#endif

#ifndef DISP_SPI
#define DISP_SPI	1		// SPI used for display
#endif

#ifndef DISP_SPI_BAUD
#define DISP_SPI_BAUD	24000000	// SPI baudrate (max. CLK_PERI/2 = 24 MHz, absolute max. 62.5 MHz)
#endif

#ifndef DISP_BLK_PIN
#define DISP_BLK_PIN	0		// backlight pin
#endif

#ifndef DISP_DC_PIN
#define DISP_DC_PIN	14		// data/command pin
#endif

#ifndef DISP_SCK_PIN
#define DISP_SCK_PIN	10		// serial clock pin
#endif

#ifndef DISP_MOSI_PIN
#define DISP_MOSI_PIN	11		// master out TX MOSI pin
#endif

#ifndef DISP_MISO_PIN
#define DISP_MISO_PIN	12		// RX MISO pin
#endif

#ifndef DISP_RES_PIN
#define DISP_RES_PIN	15		// reset pin
#endif

#ifndef DISP_CS_PIN
#define DISP_CS_PIN	13		// chip selection pin
#endif

#ifndef USE_DRAW_STDIO
#if !USE_USB_STDIO
#define USE_DRAW_STDIO	1		// use DRAW stdio (DrawPrint function)
#endif
#endif

#ifndef KEY_I2C_PORT
#define KEY_I2C_PORT    1		// I2C port for keyboard Picocalc Clockwork
#endif

#ifndef KEY_I2C_SDA
#define KEY_I2C_SDA     6		// I2C SDA pin
#endif

#ifndef KEY_I2C_SCL
#define KEY_I2C_SCL     7		// I2C SCL pin
#endif

#ifndef KEY_ADDR
#define KEY_ADDR        0x1F	// I2C address keyboard Picocalc Clockwork
#endif

#ifndef I2C_BAUDRATE
#define I2C_BAUDRATE    100000	// I2C Baudrate
#endif

// UART stdio
#ifndef UART_STDIO_PORT
#define UART_STDIO_PORT	0		// UART stdio port 0 or 1
#endif

#define PICO_DEFAULT_UART UART_STDIO_PORT // original-SDK setup

#ifndef UART_STDIO_TX
#define UART_STDIO_TX	0		// UART stdio TX GPIO pin
#endif

#ifndef UART_STDIO_RX
#define UART_STDIO_RX	1		// UART stdio RX GPIO pin
#endif

// PWM sound
#ifndef PWMSND_GPIO
#define PWMSND_GPIO	27		// PWM output GPIO pin (0..29)
#endif

#ifndef USE_PWMSND
#define USE_PWMSND	4		// use PWM sound output; set 1.. = number of channels (lib_pwmsnd.c, lib_pwmsnd.h)
#endif

// === SD card
#ifndef USE_SD
#define USE_SD		1		// use SD card (lib_sd.c, lib_sd.h)
#endif

#ifndef USE_FAT
#define USE_FAT		1		// use FAT file system (lib_fat.c, lib_fat.h)
#endif

#ifndef SD_SPI
#define SD_SPI		0		// SD card SPI interface
#endif

#ifndef SYSTICK_KEYSCAN
#define SYSTICK_KEYSCAN	 1	// call KeyScan() function from SysTick system timer
#endif

// Battery
#define BAT_PIN		29		// input from battery
#define BAT_ADC		ADC_MUX_GPIO29	// ADC input
#define BAT_MUL		3		// voltage multiplier

#ifndef USE_FATALERROR
#define USE_FATALERROR	1		// use fatal error message 0=no, 1=display LCD message (sdk_fatal.c, sdk_fatal.h)
#endif

// SD card
#ifndef SD_RX
#define SD_RX		16		// SD card RX (MISO input), to DATA_OUT pin 7
#endif

#ifndef SD_CS
#define SD_CS		17		// SD card CS, to CS pin 1
#endif

#ifndef SD_SCK
#define SD_SCK		18	// SD card SCK, to SCLK pin 5
#endif

#ifndef SD_TX
#define SD_TX		19	// SD card TX (MOSI output), to DATA_IN pin 2
#endif

// SD card speed
#ifndef SD_BAUDLOW
#define SD_BAUDLOW	250000	// SD card low baud speed (to initialize connection)
#endif

#ifndef SD_BAUD
#define SD_BAUD		4000000 // SD card baud speed (should be max. 7-12 Mbit/s; default standard bus speed
				//   is 12.5 MB/s, suggested max. bitrate 15 Mbit/s, min. writting speed 2 MB/s)
#endif

#ifndef SD_BAUDWRITE
#define SD_BAUDWRITE	1000000 // SD card baud speed of write (should be max. 7-12 Mbit/s; default standard bus speed
				//   is 12.5 MB/s, suggested max. bitrate 15 Mbit/s, min. writting speed 2 MB/s)
#endif

// LED
#define LED1	0
#define LED2	0

#endif // USE_CLOCKWORK
