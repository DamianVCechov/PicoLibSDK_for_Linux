// ****************************************************************************
//                                 
//                                Video player
//
// ****************************************************************************

#include "../../../global.h"	// globals

#include "../../../_sdk/inc/sdk_timer.h"
#include "../../../_sdk/inc/sdk_cpu.h"
#include "../../../_sdk/inc/sdk_multicore.h"
#include "lib_video_clockwork.h"
#include "../../../_lib/inc/lib_pwmsnd.h"
#include "../../../_lib/inc/lib_draw.h"
#include "../../../_display/st7789/st7789.h"
#include "../../../_display/st7365p/st7365p.h"
#include "../../../_display/minivga/minivga.h"
#include "../../../_display/disphstx/disphstx.h"
#include "../../../_lib/inc/lib_print.h"

// FIX FOR ILI9488 (320x320)
#define VIDEO_SRC_HEIGHT 240 

// frame buffer - used both to display and to load 2 video frames
ALIGNED u16 FrameBuf[VIDEO_FRAMEBUF_SIZE];

// pointer to frame to display, NULL=none
volatile u8* VideoDisp1Frame = NULL;

// request to break core1 function
volatile Bool VideoDispBreak = False;

// height to display
volatile int VideoDispHeight = HEIGHT;

// core1 function - display video frame
void VideoDispFrame()
{
	int i, j, h;
	u16* pal;
	u8 *frm, *s;
	u16 c, c2;
	u16 redmask, greenmask, bluemask;
	
	// FIX: Center 320x240 on display 320x320
	int y_offset = (HEIGHT - VIDEO_SRC_HEIGHT) / 2;

	for (;;)
	{
		// break core1 function
		if (VideoDispBreak) break;

		// check request to display video frame
		dmb();
		frm = (u8*)VideoDisp1Frame;
		if (frm != NULL)
		{
			// height to display - FORCE 240 lines for standard video
			h = VIDEO_SRC_HEIGHT;

#if USE_MINIVGA || USE_DISPHSTX
            break; // Not used for this configuration
#else // USE_MINIVGA -> SPI DISPLAY (ILI9488)

			// start sending image to the display
			DispStartImg(0, WIDTH, y_offset, y_offset + h);

			// prepare pointers
			pal = (u16*)frm; // pointer to palettes
			s = frm + 2*256; // pointer to start of image data

			redmask = 0xf800; // red mask (5 bits)
			greenmask = 0x07e0; // green mask (6 bits)
			bluemask = 0x001f; // blue mask (5 bits)

			// loop through pixels
			for (i = 0; i < h; i++)
			{
				c2 = pal[*s++];

				for (j = 0; j < WIDTH/2-1; j++)
				{
					c = c2;
					// save 1st pixel (not interpolated)
					// FIX: Big Endian Swap (High byte first)
					DispSendImg(c >> 8);
					DispSendImg(c & 0xff);

					c2 = pal[*s++]; // convert pixel to palettes

					c =	((((c & redmask) >> 1) + ((c2 & redmask) >> 1)) & redmask) | // red
						((((c & greenmask) + (c2 & greenmask)) >> 1) & greenmask) | // green
						((((c & bluemask) + (c2 & bluemask)) >> 1) & bluemask);  // blue

					// FIX: Big Endian Swap for interpolated pixel
					DispSendImg(c >> 8);
					DispSendImg(c & 0xff);
				}
				// Last pixels of the row
				DispSendImg(c >> 8);
				DispSendImg(c & 0xff);
				DispSendImg(c2 >> 8);
				DispSendImg(c2 & 0xff);
			}

			// stop sending image to the display
			DispStopImg();

#endif // USE_MINIVGA

			// info core0 that work is done
			dmb();
			VideoDisp1Frame = NULL;
		}
	}

	// signalize quit
	dmb();
	VideoDispBreak = False;
} 

// open video (returns False on error - file not found)
Bool VideoOpen(sVideo* video, const char* filename)
{
	// clear video descriptor
	memset(video, 0, sizeof(sVideo));
	video->volume = VIDEO_VOLUMEDEF;

	// ooen file
	if (!FileOpen(&video->file, filename)) return False;

	u32 frameSize = 2*256 + (WIDTH/2) * VIDEO_SRC_HEIGHT + VIDEO_SAMPLES;
	
	video->frames = video->file.size / frameSize;

	// start display service
	VideoDisp1Frame = NULL;
	VideoDispBreak = False;
	VideoDispHeight = HEIGHT;
#if USE_DISPHSTX
	DispHstxCore1Exec(VideoDispFrame);
#elif USE_MINIVGA
	VgaCore1Exec(VideoDispFrame);
#else
	Core1Exec(VideoDispFrame);
#endif
	return True;
}

// close video (must be paired with successful VideoOpen())
void VideoClose(sVideo* video)
{
 	// stop display service
	VideoDispBreak = True;
	while (VideoDispBreak) dmb();

	// stop sound
	StopSound();

	// close file
	FileClose(&video->file);
}

// repaint video control
void VideoDispCtrl(sVideo* video)
{
	char charbuf[8];
	int pos, w;

	// get control
	Bool ctrl = video->ctrl;

	// display control
	if (ctrl)
	{
		// wait to display previous frame
		while (VideoDisp1Frame != NULL) dmb();

		// clear background
		DrawRect(0, HEIGHT-VIDEO_CTRL_HEIGHT, WIDTH, VIDEO_CTRL_HEIGHT, COL_BLACK);

		// draw frame
		DrawFrame(VIDEO_CTRL_LEFT+1, HEIGHT-VIDEO_CTRL_HEIGHT+1,
			WIDTH-VIDEO_CTRL_LEFT-VIDEO_CTRL_RIGHT-2, VIDEO_CTRL_HEIGHT-2, COL_WHITE);

		// draw pointer
		w = (video->frame * (WIDTH-VIDEO_CTRL_LEFT-VIDEO_CTRL_RIGHT-2-4) / video->frames);
		DrawRect(VIDEO_CTRL_LEFT + 3, HEIGHT-VIDEO_CTRL_HEIGHT+3, w, VIDEO_CTRL_HEIGHT-6, COL_AZURE);

		// get current video position, and limit to 99:59 = 5999
		pos = VideoPos(video);
		if (pos > 5999) pos = 5999;

		// set font 8x16
		SelFont8x16();

		// display time
		MemPrint(charbuf, 7, "%02d:%02d", pos/60, pos % 60);
		DrawText(charbuf, 0, HEIGHT-VIDEO_CTRL_HEIGHT, COL_WHITE);

		// display volume
		if (video->mute)
			DrawText("X", WIDTH-VIDEO_CTRL_RIGHT+4, HEIGHT-VIDEO_CTRL_HEIGHT, COL_WHITE);
		else
		{
			MemPrint(charbuf, 7, "%02d", video->volume);
			DrawText(charbuf, WIDTH-VIDEO_CTRL_RIGHT, HEIGHT-VIDEO_CTRL_HEIGHT, COL_WHITE);
		}

		// update display
		DispUpdate();
	}
}

// play next video frame (returns False on error or end of file)
Bool VideoPlayFrame(sVideo* video)
{
	// check frame index
	if (video->frame >= video->frames) return False;

	// pointer to current frame buffer
#if USE_MINIVGA || USE_DISPHSTX	
	u8* frm = (u8*)FrameBuf + FRAMESIZE*2 + video->bufinx*VIDEO_FRAMESIZE_ALIGNED;
#else 
	u8* frm = (u8*)FrameBuf + video->bufinx*VIDEO_FRAMESIZE_ALIGNED;
#endif 
	video->bufinx ^= 1;

	u32 fileFrameSize = 2*256 + (WIDTH/2) * VIDEO_SRC_HEIGHT + VIDEO_SAMPLES;

	// read next frame (přečteme všechna data, včetně zvuku, abychom posunuli ukazatel)
	if (FileRead(&video->file, frm, fileFrameSize) != fileFrameSize) return False;

	// play sound
	if (!video->mute && !video->pause)
	{
		u32 soundOffset = 2*256 + (WIDTH/2) * VIDEO_SRC_HEIGHT;
		
		if ((video->frame == 0) || !PlayingSound())
			PlaySoundChan(0, &frm[soundOffset], VIDEO_SAMPLES, True, 1.0f,
					(float)video->volume/VIDEO_VOLUMEDEF, SNDFORM_PCM, 0);
		else
			SetNextSound(&frm[soundOffset], VIDEO_SAMPLES);
	}

	// wait to display previous frame
	while (VideoDisp1Frame != NULL) dmb();

	// wait time to next frame
	if (video->frame > 0)
	{
		while ((Time() - video->frametime) < VIDEO_TIMEDELTA) {}
	}
	video->frametime = Time();

	// increase frame number (or restore position on pause)
	if (video->pause)
	{
		// FIX: Seek using correct frame size
		FileSeek(&video->file, video->frame * fileFrameSize);
	}
	else
		video->frame++;

	// repaint video control
	VideoDispCtrl(video);

	// request to display next frame
	VideoDisp1Frame = frm;

	return True;
}

// shift relative video position in seconds
void VideoShiftPos(sVideo* video, int shift)
{
	shift *= VIDEO_FPS; // shift in frames
	int pos = shift + video->frame;
	if (pos < 0) pos = 0;
	if (pos > (int)video->frames) pos = video->frames;
	video->frame = pos;

	// FIX: Seek using correct frame size
	u32 fileFrameSize = 2*256 + (WIDTH/2) * VIDEO_SRC_HEIGHT + VIDEO_SAMPLES;
	FileSeek(&video->file, pos * fileFrameSize);

	// display control
	if (video->pause)
	{
		VideoPlayFrame(video);
		VideoDispCtrl(video);
	}
}

// video set volume 0..20
void VideoSetVol(sVideo* video, s8 vol)
{
	if (vol < 0) vol = 0;
	if (vol > VIDEO_VOLUMEMAX) vol = VIDEO_VOLUMEMAX;
	video->volume = vol;
	video->mute = False;

	// set volume
	VolumeSound((float)vol/VIDEO_VOLUMEDEF);

	// display control
	if (video->pause) VideoDispCtrl(video);
}

// video set pause
void VideoSetPause(sVideo* video, Bool pause)
{
	video->pause = pause;
	StopSound();
	if (video->pause) VideoPlayFrame(video);
}

// video set mute
void VideoSetMute(sVideo* video, Bool mute)
{
	video->mute = mute;
	StopSound();
	if (video->pause) VideoDispCtrl(video);
}

// set video control
void VideoSetCtrl(sVideo* video, Bool ctrl)
{
	// update control
	video->ctrl = ctrl;

	// wait to display previous frame
	while (VideoDisp1Frame != NULL) dmb();

	// set display height
	VideoDispHeight = ctrl ? (HEIGHT - VIDEO_CTRL_HEIGHT) : HEIGHT;

	// display control
	VideoDispCtrl(video);

	// restore screen if control is off
	if (!ctrl && video->pause) VideoPlayFrame(video);
}
