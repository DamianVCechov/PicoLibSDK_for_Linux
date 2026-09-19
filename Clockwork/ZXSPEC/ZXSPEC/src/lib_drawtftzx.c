// ****************************************************************************
//
//                   Drawing to TFT or VGA display 8-bit 332 buffer
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

#include "osdep.h"	// globals

#ifdef USE_ST7789
#include "../../../_display/st7789/st7789.h"
#endif

#ifdef USE_ST7365P
#include "../../../_display/st7365p/st7365p.h"
#endif

// Define reduced height for ZX Spectrum emulator to save RAM
#define ZX_HEIGHT 240

// Calculate vertical offset to center the display window on the PHYSICAL screen
// (HEIGHT is defined in SDK, typically 320)
#define ZX_OFFSET_Y ((HEIGHT - ZX_HEIGHT) / 2)

// Vertical adjustment to shift the image within the FRAMEBUFFER RAM.
// If the image looks "low" in the buffer (black bar at top, cut off at bottom),
// increase this value to shift the drawing coordinate UP.
#define ZX_ADJUST_Y 40

// Reduced FrameBuf size: WIDTH * ZX_HEIGHT (320 * 240)
// Přejmenováno pro zamezení kolize s FrameBuf z PicoLibSDK
ALIGNED u16 FrameBufZx_Internal[(WIDTH * ZX_HEIGHT) / 2];

u8 *FrameBufZx = (u8*) FrameBufZx_Internal;
ALIGNED u16 LineBuf[WIDTH];

#define DIRTYTILES 10
typedef struct dirty_tile {
	int X1;
	int X2;
	int Y1;
	int Y2;
	int minX;
	int maxX;
	int minY;
	int maxY;
} dirty_tile;

dirty_tile dirtyTiles[DIRTYTILES][DIRTYTILES];
int zxDispDirtyX1;
int zxDispDirtyX2;
int zxDispDirtyY1;
int zxDispDirtyY2;

// Helper to clear the FULL physical screen directly (bypassing the smaller framebuffer)
void DispClearReal() {
	// Set window to full physical display size (WIDTH x HEIGHT from SDK)
	DispWindow(0, WIDTH, 0, HEIGHT);

	// Prepare a black line buffer
	memset(LineBuf, 0, WIDTH * 2);

	// Write black lines to fill the entire screen
	for (int i = 0; i < HEIGHT; i++) {
		DispWriteData(LineBuf, WIDTH * 2);
	}
}

void InitDirtyTiles() {
	// Clear the hardware display first to remove garbage from unbuffered areas
	DispClearReal();

	int xStep = WIDTH / DIRTYTILES;
	// Use ZX_HEIGHT instead of HEIGHT for tile calculation
	int yStep = ZX_HEIGHT / DIRTYTILES;

	for (int i = 0; i < DIRTYTILES; i++) {
		for (int j = 0; j < DIRTYTILES; j++) {
			dirtyTiles[i][j].minX = i * xStep;
			dirtyTiles[i][j].maxX = (i + 1) * xStep - 1;
			dirtyTiles[i][j].minY = j * yStep;
			dirtyTiles[i][j].maxY = (j + 1) * yStep - 1;
		}
	}
	DirtyTilesNone();
}

void DirtyTilesNone() {
	for (int i = 0; i < DIRTYTILES; i++) {
		for (int j = 0; j < DIRTYTILES; j++) {
			dirtyTiles[i][j].X1 = dirtyTiles[i][j].maxX;
			dirtyTiles[i][j].X2 = dirtyTiles[i][j].minX;
			dirtyTiles[i][j].Y1 = dirtyTiles[i][j].maxY;
			dirtyTiles[i][j].Y2 = dirtyTiles[i][j].minY;
		}
	}
	zxDispDirtyX1 = WIDTH; //DispWidth;
	zxDispDirtyX2 = 0;
	zxDispDirtyY1 = ZX_HEIGHT; // Use ZX_HEIGHT
	zxDispDirtyY2 = 0;
}

void DirtyTilesAll() {
	for (int i = 0; i < DIRTYTILES; i++) {
		for (int j = 0; j < DIRTYTILES; j++) {
			dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
			dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX;
			dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
			dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY;
		}
	}
	zxDispDirtyX1 = 0;
	zxDispDirtyX2 = WIDTH; //DispWidth;
	zxDispDirtyY1 = 0;
	zxDispDirtyY2 = ZX_HEIGHT; // Use ZX_HEIGHT
}

// update dirty area by pixel (check valid range)
void DispDirtyPointTiles(int x, int y) {
	// Check against ZX_HEIGHT
	if (((u32) x < (u32) WIDTH) && ((u32) y < (u32) ZX_HEIGHT)) {
		int xStep = WIDTH / DIRTYTILES;
		int yStep = ZX_HEIGHT / DIRTYTILES; // Use ZX_HEIGHT

		int tileX1 = (x + 1) / xStep + ((x + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY1 = (y + 1) / yStep + ((y + 1) % yStep > 0 ? 1 : 0) - 1;

		if (x < dirtyTiles[tileX1][tileY1].X1)
			dirtyTiles[tileX1][tileY1].X1 = x;
		if (x + 1 > dirtyTiles[tileX1][tileY1].X2)
			dirtyTiles[tileX1][tileY1].X2 = x + 1;
		if (y < dirtyTiles[tileX1][tileY1].Y1)
			dirtyTiles[tileX1][tileY1].Y1 = y;
		if (y + 1 > dirtyTiles[tileX1][tileY1].Y2)
			dirtyTiles[tileX1][tileY1].Y2 = y + 1;

		if (x < zxDispDirtyX1)
			zxDispDirtyX1 = x;
		if (x + 1 > zxDispDirtyX2)
			zxDispDirtyX2 = x + 1;
		if (y < zxDispDirtyY1)
			zxDispDirtyY1 = y;
		if (y + 1 > zxDispDirtyY2)
			zxDispDirtyY2 = y + 1;

	}
}

// update dirty area by rectangle (check valid range)
void DispDirtyRectTiles(int x, int y, int w, int h) {
	int xStep = WIDTH / DIRTYTILES;
	int yStep = ZX_HEIGHT / DIRTYTILES; // Use ZX_HEIGHT

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (x + w > WIDTH)
		w = WIDTH - x;
	if (w <= 0)
		return;

	if (y < 0) {
		h += y;
		y = 0;
	}
	// Check against ZX_HEIGHT
	if (y + h > ZX_HEIGHT)
		h = ZX_HEIGHT - y;
	if (h <= 0)
		return;

	int tileX1 = (x + 1) / xStep + ((x + 1) % xStep > 0 ? 1 : 0) - 1;
	int tileY1 = (y + 1) / yStep + ((y + 1) % yStep > 0 ? 1 : 0) - 1;

	int tileX2 = (x + w + 1) / xStep + ((x + w + 1) % xStep > 0 ? 1 : 0) - 1;
	int tileY2 = (y + h + 1) / yStep + ((y + h + 1) % yStep > 0 ? 1 : 0) - 1;

	if (tileX2 >= DIRTYTILES) {
		tileX2 = DIRTYTILES - 1;
	}
	if (tileY2 >= DIRTYTILES) {
		tileY2 = DIRTYTILES - 1;
	}
	for (int i = tileX1; i <= tileX2; i++) {
		for (int j = tileY1; j <= tileY2; j++) {

			if (i == tileX1) {
				//jsem v pocatecnim tile X
				if (x < dirtyTiles[i][j].X1)
					dirtyTiles[i][j].X1 = x;
				if ((x + w) > dirtyTiles[i][j].maxX)
					dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX + 1;
			}
			if (i == tileX2) {
				//jsem v koncovem tile X
				if ((x + w) > dirtyTiles[i][j].X2)
					dirtyTiles[i][j].X2 = x + w;
				if (x < dirtyTiles[i][j].minX)
					dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
			}
			if ((i != tileX1) && (i != tileX2)) {

				//jsem v prostrednim tile X
				dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
				dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX + 1;
			}

			if (j == tileY1) {
				//jsem v pocatecnim tile Y
				if (y < dirtyTiles[i][j].Y1)
					dirtyTiles[i][j].Y1 = y;
				if ((y + h) > dirtyTiles[i][j].maxY)
					dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY + 1;

			}
			if (j == tileY2) {
				//jsem v koncovem tile Y
				if ((y + h) > dirtyTiles[i][j].Y2)
					dirtyTiles[i][j].Y2 = y + h;
				if (y < dirtyTiles[i][j].minY)
					dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
			}
			if ((j != tileY1) && (j != tileY2)) {
				//jsem v prostrednim tile Y
				dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
				dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY + 1;
			}

		}

	}

	if (x < zxDispDirtyX1)
		zxDispDirtyX1 = x;
	if (x + w > zxDispDirtyX2)
		zxDispDirtyX2 = x + w;
	if (y < zxDispDirtyY1)
		zxDispDirtyY1 = y;
	if (y + h > zxDispDirtyY2)
		zxDispDirtyY2 = y + h;
}

// update - send dirty window to display
void DispUpdateZx() {
	int nSolid = 0;

	// 1. Výpočet nSolid a souřadnic
	if ((zxDispDirtyX1 < zxDispDirtyX2) && (zxDispDirtyY1 < zxDispDirtyY2)) {
		int xStep = WIDTH / DIRTYTILES;
		int yStep = ZX_HEIGHT / DIRTYTILES; // Use ZX_HEIGHT
		int tileX1 = (zxDispDirtyX1 + 1) / xStep + ((zxDispDirtyX1 + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY1 = (zxDispDirtyY1 + 1) / yStep + ((zxDispDirtyY1 + 1) % yStep > 0 ? 1 : 0) - 1;
		int tileX2 = (zxDispDirtyX2 + 1) / xStep + ((zxDispDirtyX2 + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY2 = (zxDispDirtyY2 + 1) / yStep + ((zxDispDirtyY2 + 1) % yStep > 0 ? 1 : 0) - 1;

		if (tileX2 >= DIRTYTILES) tileX2 = DIRTYTILES - 1;
		if (tileY2 >= DIRTYTILES) tileY2 = DIRTYTILES - 1;

		tileX1++; tileY1++;
		tileX2--; tileY2--;

		if ((tileX1 < tileX2) && (tileY1 < tileY2)) {
			nSolid = 1;
			for (int i = tileX1; (i <= tileX2) && (nSolid == 1); i++) {
				for (int j = tileY1; (j <= tileY2) && (nSolid == 1); j++) {
					if (!((dirtyTiles[i][j].X1 <= dirtyTiles[i][j].minX) && (dirtyTiles[i][j].X2 >= dirtyTiles[i][j].maxX) && (dirtyTiles[i][j].Y1 <= dirtyTiles[i][j].minY)
						&& (dirtyTiles[i][j].Y2 >= dirtyTiles[i][j].maxY))) {
						nSolid = 0;
						}
				}
			}
		}
	}

	// 2. Vykreslování
	if (nSolid == 1) {
		// --- CELISTVÝ BLOK ---
		// Add ZX_OFFSET_Y to center the display window on the physical screen
		DispWindow((u16) zxDispDirtyX1, (u16) zxDispDirtyX2, (u16) zxDispDirtyY1 + ZX_OFFSET_Y, (u16) zxDispDirtyY2 + ZX_OFFSET_Y);

		u8 *s0 = &FrameBufZx[zxDispDirtyX1 + zxDispDirtyY1 * WIDTH];
		int width = zxDispDirtyX2 - zxDispDirtyX1;

		for (int i = zxDispDirtyY2 - zxDispDirtyY1; i > 0; i--) {
			// Provedení přehození bajtů pro ST7365P (Big-Endian formát)
			for (int nCnt = 0; nCnt < width; nCnt++) {
				u16 color = RGB8TO16(s0[nCnt]);
				LineBuf[nCnt] = (color << 8) | (color >> 8); // Swap bytes
			}
			DispWriteData(LineBuf, width * 2);
			s0 += WIDTH;
		}
	} else {
		// --- PO DLAŽDICÍCH ---
		for (int j = 0; j < DIRTYTILES; j++) {
			for (int i = 0; i < DIRTYTILES; i++) {
				if ((dirtyTiles[i][j].X1 < dirtyTiles[i][j].X2) && (dirtyTiles[i][j].Y1 < dirtyTiles[i][j].Y2)) {

					// Add ZX_OFFSET_Y to center the tile window on the physical screen
					DispWindow((u16) dirtyTiles[i][j].X1, (u16) dirtyTiles[i][j].X2, (u16) dirtyTiles[i][j].Y1 + ZX_OFFSET_Y, (u16) dirtyTiles[i][j].Y2 + ZX_OFFSET_Y);

					u8 *s0 = &FrameBufZx[dirtyTiles[i][j].X1 + dirtyTiles[i][j].Y1 * WIDTH];
					int width = dirtyTiles[i][j].X2 - dirtyTiles[i][j].X1;

					for (int k = dirtyTiles[i][j].Y2 - dirtyTiles[i][j].Y1; k > 0; k--) {
						// Provedení přehození bajtů pro ST7365P (Big-Endian formát)
						for (int nCnt = 0; nCnt < width; nCnt++) {
							u16 color = RGB8TO16(s0[nCnt]);
							LineBuf[nCnt] = (color << 8) | (color >> 8); // Swap bytes
						}
						DispWriteData(LineBuf, width * 2);
						s0 += WIDTH;
					}
				}
			}
		}
	}
	DirtyTilesNone();
}

// Draw character (transparent background)
void DrawCharZx(char ch, int x, int y, u8 col) {
	int x0 = x;
	int i, j;
	u8 *d;

	// Shift Y into the RAM buffer
	int yRel = y - DispMinY - ZX_ADJUST_Y;

	// prepare pointer to font sample
	const u8 *s = &pDrawFont[ch];

	// check if drawing is safe to use fast drawing
	// Use ZX_HEIGHT instead of DispMaxY to ensure we don't write outside the smaller buffer
	if ((x >= 0) && (yRel >= 0) && (x + DrawFontWidth <= WIDTH) && (yRel + DrawFontHeight <= ZX_HEIGHT)) {
		// update dirty rectangle
		DispDirtyRectTiles(x, yRel, DrawFontWidth, DrawFontHeight);

		// destination address
		d = &FrameBufZx[yRel * WIDTH + x];

		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			for (j = DrawFontWidth; j > 0; j--) {
				// draw pixel
				if ((ch & 0x80) != 0)
					*d = col;
				d++;
				ch <<= 1;
			}
			d += WIDTH - DrawFontWidth;
		}
	}

	// use slow safe drawing
	else {
		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			x = x0;
			for (j = DrawFontWidth; j > 0; j--) {
				// pixel is set
				if ((ch & 0x80) != 0)
					DrawPointZx(x, y, col); // DrawPointZx handles adjustment
				x++;
				ch <<= 1;
			}
			y++;
		}
	}
}

// Draw character with background
void DrawCharBg(char ch, int x, int y, u8 col, u8 bgcol) {
	int x0 = x;
	int i, j;
	u8 c;
	u8 *d;

	// Shift Y into the RAM buffer
	int yRel = y - DispMinY - ZX_ADJUST_Y;

	// prepare pointer to font sample
	const u8 *s = &pDrawFont[ch];

	// check if drawing is safe to use fast drawing
	// Use ZX_HEIGHT instead of DispMaxY
	if ((x >= 0) && (yRel >= 0) && (x + DrawFontWidth <= WIDTH) && (yRel + DrawFontHeight <= ZX_HEIGHT)) {
		// update dirty rectangle
		DispDirtyRectTiles(x, yRel, DrawFontWidth, DrawFontHeight);

		// destination address
		d = &FrameBufZx[yRel * WIDTH + x];

		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			for (j = DrawFontWidth; j > 0; j--) {
				// draw pixel
				*d++ = ((ch & 0x80) != 0) ? col : bgcol;
				ch <<= 1;
			}
			d += WIDTH - DrawFontWidth;
		}
	}

	// use slow safe drawing
	else {
		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			x = x0;
			for (j = DrawFontWidth; j > 0; j--) {
				// pixel is set
				c = ((ch & 0x80) != 0) ? col : bgcol;

				// draw pixel
				DrawPointZx(x, y, c);
				x++;
				ch <<= 1;
			}
			y++;
		}
	}
}

// Draw text (transparent background)
void DrawTextZx(const char *text, int x, int y, u8 col) {
	u8 ch;

	// loop through characters of text
	for (;;) {
		// get next character of the text
		ch = (u8) *text++;
		if (ch == 0)
			break;

		// draw character
		DrawCharZx(ch, x, y, col);

		// shift to next character position
		x += DrawFontWidth;
	}
}

// Draw text with background
void DrawTextBgZx(const char *text, int x, int y, u8 col, u8 bgcol) {
	u8 ch;

	// loop through characters of text
	for (;;) {
		// get next character of the text
		ch = (u8) *text++;
		if (ch == 0)
			break;

		// draw character
		DrawCharBg(ch, x, y, col, bgcol);

		// shift to next character position
		x += DrawFontWidth;
	}
}

// Draw character double sized (transparent background)
void DrawChar2Zx(char ch, int x, int y, u8 col) {
	int x0 = x;
	int i, j;
	u8 *d;
	int w = WIDTH;

	// Shift Y into the RAM buffer
	int yRel = y - DispMinY - ZX_ADJUST_Y;

	// prepare pointer to font sample
	const u8 *s = &pDrawFont[ch];

	// check if drawing is safe to use fast drawing
	// Use ZX_HEIGHT instead of DispMaxY
	if ((x >= 0) && (yRel >= 0) && (x + 2 * DrawFontWidth <= w) && (yRel + 2 * DrawFontHeight <= ZX_HEIGHT)) {
		// update dirty rectangle
		DispDirtyRectTiles(x, yRel, 2 * DrawFontWidth, 2 * DrawFontHeight);

		// destination address
		d = &FrameBufZx[yRel * w + x];

		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			for (j = DrawFontWidth; j > 0; j--) {
				// draw pixel
				if ((ch & 0x80) != 0) {
					*d = col;
					d[1] = col;
					d[w] = col;
					d[w + 1] = col;
				}
				d += 2;
				ch <<= 1;
			}
			d += 2 * w - 2 * DrawFontWidth;
		}
	}

	// use slow safe drawing
	else {
		// loop through lines of one character
		for (i = DrawFontHeight; i > 0; i--) {
			// get one font sample
			ch = *s;
			s += 256;

			// loop through pixels of one character line
			x = x0;
			for (j = DrawFontWidth; j > 0; j--) {
				// pixel is set
				if ((ch & 0x80) != 0) {
					DrawPointZx(x, y, col);
					DrawPointZx(x + 1, y, col);
					DrawPointZx(x, y + 1, col);
					DrawPointZx(x + 1, y + 1, col);
				}
				x += 2;
				ch <<= 1;
			}
			y += 2;
		}
	}
}

// Draw text double sized (transparent background)
void DrawText2Zx(const char *text, int x, int y, u8 col) {
	u8 ch;

	// loop through characters of text
	for (;;) {
		// get next character of the text
		ch = (u8) *text++;
		if (ch == 0)
			break;

		// draw character
		DrawChar2Zx(ch, x, y, col);

		// shift to next character position
		x += DrawFontWidth * 2;
	}
}

// Draw point
void DrawPointZx(int x, int y, u8 col) {
	// Shift Y into the RAM buffer
	int yRel = y - DispMinY - ZX_ADJUST_Y;

	// check coordinates
	// Use ZX_HEIGHT check and adjusted Y
	if (((u32) x >= (u32) WIDTH) || (yRel < 0) || (yRel >= ZX_HEIGHT))
		return;

	// draw pixel
	FrameBufZx[x + yRel * WIDTH] = col;

	// update dirty area by rectangle (must be in valid limits)
	DispDirtyPointTiles(x, yRel);
}

// draw rectangle
void DrawRectZx(int x, int y, int w, int h, u8 col) {
	// limit x
	if (x < 0) {
		w += x;
		x = 0;
	}

	// limit w
	if (x + w > WIDTH)
		w = WIDTH - x;
	if (w <= 0)
		return;

	// Apply shift to Y
	int yRel = y - DispMinY - ZX_ADJUST_Y;

	// limit y
	if (yRel < 0) {
		h += yRel; // Decrease height by amount cut off from top
		yRel = 0;
	}

	// limit h
	// Use ZX_HEIGHT
	if (yRel + h > ZX_HEIGHT)
		h = ZX_HEIGHT - yRel;
	if (h <= 0)
		return;

	// update dirty rectangle
	DispDirtyRectTiles(x, yRel, w, h);

	// draw
	u8 *d = &FrameBufZx[x + yRel * WIDTH];
	int wb = WIDTH - w;
	int i;
	for (; h > 0; h--) {
		for (i = w; i > 0; i--)
			*d++ = col;
		d += wb;
	}
}

// Draw frame
void DrawFrameZx(int x, int y, int w, int h, u8 col) {
	if ((w <= 0) || (h <= 0))
		return;
	DrawRectZx(x, y, w - 1, 1, col);
	DrawRectZx(x + w - 1, y, 1, h - 1, col);
	DrawRectZx(x + 1, y + h - 1, w - 1, 1, col);
	DrawRectZx(x, y + 1, 1, h - 1, col);
}

// clear canvas with color
void DrawClearColZx(u8 col) {
	// Clear only up to ZX_HEIGHT
	// Internally DrawRectZx handles alignment, but we want to clear the whole BUFFER
	// So we manually clear the buffer range 0..ZX_HEIGHT
	// Note: DispClearReal handles the physical screen cleaning.
	// This function clears the logic buffer.
	
	memset(FrameBufZx, col, WIDTH * ZX_HEIGHT);
	DirtyTilesAll();
}

// clear canvas with black color
void DrawClearZx() {
	DrawClearColZx(COL_BLACK_ZX);
}
