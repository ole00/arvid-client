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


#define MAX_W 320
#define MAX_H 240
#define TOTAL_H 288

unsigned short fb[MAX_W * TOTAL_H];


int main(int argc, char** argv) {
	int lines = 0;
	int result = 0;
	int modifier = 0;
	char* serverAddr = "192.168.2.101";

	printf("connecting to %s ...\n", serverAddr);
	result = arvid_client_connect(serverAddr);
	if (result < 0) {
		return 1;
	}
	printf("connected!\n");

	lines = arvid_client_get_video_mode_lines(ARVID_320, 60.0f);
	printf("lines: %i\n", lines);


	if (argc > 1) {
		modifier = atoi(argv[1]);
	}
	printf("setting line sync modifier: %i\n", modifier);
	printf("current mod: %i\n", arvid_client_get_line_pos_mod());
	arvid_client_set_line_pos_mod((short)modifier);


	arvid_client_set_video_mode(ARVID_320, lines);
}

