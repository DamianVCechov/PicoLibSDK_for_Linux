#ifndef _CONFIG_H
#define _CONFIG_H

#define USE_EMU             1
#define USE_EMU_M68K        1
#define USE_SD              1
#define USE_FAT             1
#define USE_FLASH           1
#define USE_PWMSND          0
#define USE_USBPAD          0
#define USE_USB_HOST_HID    0

// Keep the ROM after the application image, aligned for FlashErase().
#define TI92_ROM_XIP_BASE ((const u8*)(((u32)&__etext + \
	(u32)&__data_end__ - (u32)&__data_start__ + 0xfff + 256 + 4096) & ~0xfff))

#include CONFIG_DEF_H

#endif // _CONFIG_H
