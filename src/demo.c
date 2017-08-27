/*
Arvid software and hardware is licensed under MIT license:

Copyright (c) 2015 Marek Olejnik

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


#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>


#include "arvid_client.h"

#define VIDEO_MODE ARVID_320
#define MAX_W 320
#define MAX_H 240
#define TOTAL_H 288

unsigned short fb[MAX_W * TOTAL_H];

int posX, posY;
int incX, incY;

int topOffset;


static void fillRect(int x, int y, int w, int h, unsigned short color) {
	int j, i;
	unsigned short* pix;
	y +=topOffset;

	pix = fb + (y * MAX_W) + x;
	for (j = 0; j < h; j ++) {
		for (i = 0; i < w; i++) {
			pix[i] = color;
		}
		pix += MAX_W;
	}
}

static void cleanFrames() {
	topOffset = 0;
	fillRect(0, 0, MAX_W, TOTAL_H, 0);
	arvid_client_wait_for_vsync();
	arvid_client_blit_buffer(fb, MAX_W, TOTAL_H, MAX_W);
	arvid_client_wait_for_vsync();
	arvid_client_blit_buffer(fb, MAX_W, TOTAL_H, MAX_W);

	topOffset = 32;
}

static void renderFrame(void) {
	int index;
	unsigned long long time;
	static unsigned long long timeTotal = 0;
	static unsigned long long sizeTotal = 0;
	static int timeCnt = 0;

	posX += incX;
	if (posX < 0 || posX > MAX_W - 32) {
		incX = -incX;
		posX += incX * 2;
	}

	posY += incY;	
	if (posY < 0 || posY > MAX_H - 32) {
		incY = -incY;
		posY += incY * 2;
	}

	fillRect(0, 0, MAX_W, MAX_H, 0x432);
	fillRect(posX, posY, 32, 32, 0xFFF);

	index = arvid_client_wait_for_vsync();
	arvid_client_blit_buffer(fb, MAX_W, MAX_H + topOffset, MAX_W);
	
}

int main(int argc, char** argv) {
	int lines = 0;
	int result = 0;
	char* serverAddr = "192.168.2.101";
	if (argc > 1) {
	    serverAddr = argv[1];
	}

	printf("connecting to %s ...\n", serverAddr);
	result = arvid_client_connect(serverAddr);
	if (result < 0) {
		return 1;
	}
	printf("connected!\n");

	lines = arvid_client_get_video_mode_lines(VIDEO_MODE, 60.0f);

	arvid_client_set_video_mode(VIDEO_MODE, lines);

	//arvid_client_set_virtual_vsync(32);

	posX = 100;
	posY = 100;
	incX = 2;
	incY = 2;

	cleanFrames();

	while (1) {
		renderFrame();
	}
}

