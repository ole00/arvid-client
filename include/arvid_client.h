/*
Arvid software and hardware is licensed under MIT license:

Copyright (c) 2015 - 2017 Marek Olejnik

Permission is hereby granted, free of charge, to any person obtaining a copy
of this hardware, software, and associated documentation files (the "Product"),
to deal in the Product without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Product, and to permit persons to whom the Product is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Product.

THE PRODUCT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE PRODUCT OR THE USE OR OTHER DEALINGS
IN THE PRODUCT.

*/


#ifndef _ARVID_CLIENT_H_
#define _ARVID_CLIENT_H_

/* video mode numbers/ids */
#define ARVID_320 0
#define ARVID_256 1
#define ARVID_288 2
#define ARVID_384 3
#define ARVID_240 4
#define ARVID_392 5
#define ARVID_400 6
#define ARVID_292 7
#define ARVID_336 8
#define ARVID_416 9
#define ARVID_448 10
#define ARVID_512 11
#define ARVID_640 12

#define ARVID_BLIT_TYPE_BLOCKING 0
#define ARVID_BLIT_TYPE_NON_BLOCKING 1

#define ARVID_TATE_SWITCH (1 << 19)
#define ARVID_COIN_BUTTON (1 << 17)
#define ARVID_START_BUTTON (1 << 21)

// new switch & button definition
#define ARVID_TEST_SWITCH (1 << 24)
#define ARVID_SERVICE_SWITCH (1 << 20)
#define ARVID_TILT_SWITCH (1 << 14)

#define ARVID_P1_COIN (1 << 17)
#define ARVID_P1_START (1 << 21)
#define ARVID_P1_UP (1 << 7)
#define ARVID_P1_DOWN (1 << 25)
#define ARVID_P1_LEFT (1 << 3)
#define ARVID_P1_RIGHT (1 << 2)
#define ARVID_P1_B1 (1 << 5)
#define ARVID_P1_B2 (1 << 4)
#define ARVID_P1_B3 (1 << 31)
#define ARVID_P1_B4 (1 << 30)

#define ARVID_P2_COIN (1 << 16)
#define ARVID_P2_START (1 << 15)
#define ARVID_P2_UP (1 << 8)
#define ARVID_P2_DOWN (1 << 9)
#define ARVID_P2_LEFT (1 << 10)
#define ARVID_P2_RIGHT (1 << 11)
#define ARVID_P2_B1 (1 << 22)
#define ARVID_P2_B2 (1 << 27)
#define ARVID_P2_B3 (1 << 26)
#define ARVID_P2_B4 (1 << 23)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct arvid_client_vmode_info_s {
	unsigned short width; /* width in pixels */
	unsigned short vmode; /* video mode number/id */
} arvid_client_vmode_info; 

/* connects the client to the arvid-server */
int arvid_client_connect(char* serverAddress);

/* closes arvid */
int arvid_client_close(void);

/* sets the blitting type

* Blocking : blit_buffer function waits till the frame buffer
   is fully compressed and sent. It uses all CPU cores to compress.
   It is safe to use the same buffer when calling blit_buffer
   function subsequently. This is the default blitting type and
   should work the best for 2 and less CPU cores.

* Non-blocking: blit_buffer function return ASAP. It uses
   (MAX - 1) CPU cores, so the main thread can continue producing 
   new frame buffer content. The frame buffer is compressed and sent 
   in parallel with the main thread, so it is unsafe to use the same 
   buffer when calling blit_buffer function subsequently. You should 
   use at least 2 distinct buffers and alternate them while
   calling the blit_buffer function. In this mode the wait_for_vsync
   function first waits till the whole framebuffer is compressed
   and transferred, then issues vsync wait.

Blit type is set to default when you call the connect function.
Note: NON_BLOCKING type is impossible to set if running on single 
  core CPU.

Returns 0 on success, negative on failure (invalid argument etc.)
*/
int arvid_client_set_blit_type(int type);

/* sends the video frame to hidden arvid buffer 
	returns 0 on success, -1 on failure
*/
int arvid_client_blit_buffer(unsigned short* buffer, int width, int height,  int stride);

/* returns current frame number */
unsigned int arvid_client_get_frame_number(void);

/* waits for a vsync then returns current frame number */
unsigned int arvid_client_wait_for_vsync(void);

/* sets arvid video mode 
	return 0 on success
*/
int arvid_client_set_video_mode(int mode, int lines);


/* returns the number of video lines (vertical resolution)
	of specified videomode */
int arvid_client_get_video_mode_lines(int mode, float frequency);

/* returns the screen refresh rate for particular video mode
	and number of lines of that video mode */
float arvid_client_get_video_mode_refresh_rate(int mode, int lines);

/* enumerates available video modes 
 * returns number of videomodes on success, -1 on error
 * vmodes and maxItem must not be NULL
 * maxItem must be set to number of items the vmodes buffer can hold.
 */
int arvid_client_enum_video_modes(arvid_client_vmode_info* vmodes, int maxItem);

/* get the width in pixels of current video mode 
	or negative number on error */
int arvid_client_get_width(void);

/* get number of lines of current video mode 
	or negative number on error */
int arvid_client_get_height(void);


/* get button state (coins and switches) */
int arvid_client_get_button_status(void);

/* Gets a number of bytes transferred to server in between
the calls of this function */
unsigned int arvid_client_get_stat_transferred_size(void);

/* set the line position modifier */
int arvid_client_set_line_pos_mod(short mod);

/* get the line position modifier */ 
int arvid_client_get_line_pos_mod(void);

/* set virtual vsync line. use -1 to disable */
int arvid_client_set_virtual_vsync(int vsyncLine);

/* sends an update file to the server */
int arvid_client_update_server(unsigned char* updateData, int updateSize);

/**
Issues power-off command to the arvid server.
This closes arvid connection and casues arvid server to power off.
*/
int arvid_client_power_off_server(void);


#ifdef __cplusplus
}
#endif


#endif
