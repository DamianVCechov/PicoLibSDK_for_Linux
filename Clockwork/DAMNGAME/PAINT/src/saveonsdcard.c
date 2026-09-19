
// ****************************************************************************
//
//                            PicoPad Screen Shot
//
// ****************************************************************************
// PicoLibSDK - Alternative SDK library for Raspberry Pico and RP2040
// Copyright (c) 2023 Miroslav Nemecek, Panda38@seznam.cz, hardyplotter2@gmail.com
// 	https://github.com/Panda381/PicoLibSDK
//	https://www.breatharian.eu/hw/picolibsdk/index_en.html
//	https://github.com/pajenicko/picopad
//	https://picopad.eu/en/
// License:
//	This source code is freely available for any purpose, including commercial.
//	It is possible to take and modify the code or parts of it, without restriction.

#include "../include.h"
#include "saveonsdcard.h"

#define DIRPATH	"/PAINT/" // save directory
/*
// BMP file header (size 70 bytes)
#pragma pack(push,1)
typedef struct {
	// BMP file header (size 14 bytes)
	u16	bfType;			// 0x00: magic, 'B' 'M' = 0x4D42
	u32	bfSize;			// 0x02: file size, aligned to DWORD = 70 + 320*240*2 + 2 = 153672 = 0x25848
	u16	bfReserved1;		// 0x06: = 0
	u16	bfReserved2;		// 0x08: = 0
	u32	bfOffBits;		// 0x0A: offset of data bits after file header = 70 (0x46)
	// BMP info header (size 40 bytes)
	u32	biSize;			// 0x0E: size of this info header = 56 (0x38)
	s32	biWidth;		// 0x12: width = 320 (0x140)
	s32	biHeight;		// 0x16: height, negate if flip row order = -240 (0xFFFFFF10)
	u16	biPlanes;		// 0x1A: planes = 1
	u16	biBitCount;		// 0x1C: number of bits per pixel = 16
	u32	biCompression;		// 0x1E: compression = 3 (BI_BITFIELDS)
	u32	biSizeImage;		// 0x22: size of data of image + aligned file = 320*240*2 + 2 = 153602 (0x25802)
	s32	biXPelsPerMeter;	// 0x26: X pels per meter = 2834 (= 0xB12)
	s32	biYPelsPerMeter;	// 0x2A: Y pels per meter = 2834 (= 0xB12)
	u32	biClrUsed;		// 0x2E: number of user colors (0 = all)
	u32	biClrImportant;		// 0x32: number of important colors (0 = all)
	// BMP color bit mask (size 16 bytes)
	u32	biRedMask;		// 0x36: red mask = 0x0000F800
	u32	biGreenMask;		// 0x3A: green mask = 0x000007E0
	u32	biBlueMask;		// 0x3E: blue mask = 0x0000001F
	u32	biAlphaMask;		// 0x42: alpha mask = 0x00000000
					// 0x46
} sBmp;
#pragma pack(pop)
*/
// BMP file header, 16 bits per pixel
sBmp BmpHeader = {
	// BMP file header (size 14 bytes)
	0x4D42,			// u16	bfType;		// 0x00: magic, 'B' 'M' = 0x4D42
	70 + WIDTH*HEIGHT*2 + 2, // u32	bfSize;		// 0x02: file size, aligned to DWORD = 70 + 320*240*2 + 2 = 153672 = 0x25848
	0,			// u16	bfReserved1;	// 0x06: = 0
	0,			// u16	bfReserved2;	// 0x08: = 0
	70,			// 0x0A: offset of data bits after file header = 70 (0x46)
	// BMP info header (size 40 bytes)
	56,			// u32	biSize;		// 0x0E: size of this info header = 56 (0x38)
	WIDTH,			// s32	biWidth;	// 0x12: width = 320 (0x140)
	-HEIGHT,		// s32	biHeight;	// 0x16: height, negate if flip row order = -240 (0xFFFFFF10)
	1,			// u16	biPlanes;	// 0x1A: planes = 1
	16,			// u16	biBitCount;	// 0x1C: number of bits per pixel = 16
	3,			// u32	biCompression;	// 0x1E: compression = 3 (BI_BITFIELDS)
	WIDTH*HEIGHT*2+2,	// u32	biSizeImage;	// 0x22: size of data of image + aligned file = 320*240*2 + 2 = 153602 (0x25802)
	2834,			// s32	biXPelsPerMeter; // 0x26: X pels per meter = 2834 (= 0xB12)
	2834,			// s32	biYPelsPerMeter; // 0x2A: Y pels per meter = 2834 (= 0xB12)
	0,			// u32	biClrUsed;	// 0x2E: number of user colors (0 = all)
	0,			// u32	biClrImportant;	// 0x32: number of important colors (0 = all)
	// BMP color bit mask (size 16 bytes)
	0xF800,			// u32	biRedMask;	// 0x36: red mask = 0x0000F800
	0x07E0,			// u32	biGreenMask;	// 0x3A: green mask = 0x000007E0
	0x001F,			// u32	biBlueMask;	// 0x3E: blue mask = 0x0000001F
	0,			// u32	biAlphaMask;	// 0x42: alpha mask = 0x00000000
							// 0x46
};

void SavePicture()
{
	int i;
	char name[30];
	sFile file;

	// mounted flag
	Bool ismount = DiskMounted();

	// auto-mount disk
	if (DiskAutoMount())
	{

		for (i = 1; i < 10000; i++)
		{
			MemPrint(name, 30, "%s%d.BMP", DIRPATH, i);
			if (!FileOpen(&file, name)) break;
		}

		if (FileCreate(&file, name))
		{
			// write BMP file header
			FileWrite(&file, &BmpHeader, sizeof(sBmp));

			// write image data
			FileWrite(&file, FrameBuf, WIDTH*HEIGHT*2+2);

			// close file
			FileClose(&file);

			// flush disk writes
			DiskFlush();
		}
	}

	// unmount disk
	if (!ismount) DiskUnmount();
}
