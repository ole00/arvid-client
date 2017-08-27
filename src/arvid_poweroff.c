/*
Arvid software and hardware is licensed under MIT license:

Copyright (c) 2017 Marek Olejnik

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

// This programm allows you to send firmware update file to Arvid.
// The file is a .tgz file containing updated/changed arvid files.
// After the upload the firmware file is stored on BBG and
// after power-cycle the archive is extracted into /opt/libarvid
// directory.  Requires at least Arvid 0.4c to function.

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>

#ifdef MINGW
#include <windows.h>
#else
#include <unistd.h>
#endif


#include "arvid_client.h"
#include "crc.h"


#define DEFAULT_ADDR "192.168.2.101"
#define MAX_SIZE 0x200000

char* serverAddr = DEFAULT_ADDR;
char doPrintHelp = 0;

static void sleep_(int ms) {
	#ifdef MINGW
	Sleep(ms);
	#else
	usleep(ms * 1000);
	#endif
	
}

static void checkParams(int argc, char** argv) {
	int i;
	for (i = 0 ; i < argc; i++) {
		char* arg = argv[i];
		if (strcmp(arg, "-addr") == 0) {
			serverAddr = argv[++i];
		} else
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "-?") == 0) {
			doPrintHelp =1;
		}
	}
} 

static void printHelp(void) {
	printf("Arvid remote poweroff tool.\n");
	printf("usage: arvid_poweroff [-addr arvid_ip_address]\n");
}

static void closeArvid(void) {
	arvid_client_close();
}
int main(int argc, char** argv) {
	int lines = 0;
	unsigned char* data = 0;
	int result = 0;

	checkParams(argc, argv);
	if (doPrintHelp) {
		printHelp();
		return -1;
	}

	printf("connecting to %s ...\n", serverAddr);
	result = arvid_client_connect(serverAddr);
	if (result < 0) {
		return 1;
	}
	printf("connected!\n");
	sleep_(500);
	result = arvid_client_power_off_server();
	printf("Done. result code=%i.\n", result);
	return 0;
}

