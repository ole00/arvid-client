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


static void sleep_(int ms) {
	#ifdef MINGW
	Sleep(ms);
	#else
	usleep(ms * 1000);
	#endif
	
}


int readFile(char* fname, unsigned char** data) {
	
	FILE* f;
	int size;
	unsigned char* d = (unsigned char*) malloc(MAX_SIZE);
	if (d == 0) {
		return -1;
	}
	*data = d;
	f = fopen(fname, "r");
	if (f == 0) {
		return -2;
	}
	size = fread(d, 1, MAX_SIZE, f);
	fclose(f);
	return size;
}

static void checkParams(int argc, char** argv) {
	int i;
	for (i = 0 ; i < argc; i++) {
		char* arg = argv[i];
		if (strcmp(arg, "-addr") == 0) {
			serverAddr = argv[++i];
		}
	}
} 

static void printHelp(void) {
	printf("Arvid firmware upload tool.\n");
	printf("usage: fw_upload fw_file_name.tgz [-addr arvid_ip_address]\n");
}

static int waitForCoin(void) {
	int i;
	for (i = 0; i < 20; i++) {
		arvid_client_wait_for_vsync();
		int button = arvid_client_get_button_status();
		if (button & ARVID_COIN_BUTTON) {
			return 0;
		}
		sleep_(500);
	}
	return -1;
}

static void closeArvid(void) {
	arvid_client_close();
}
int main(int argc, char** argv) {
	int lines = 0;
	unsigned char* data = 0;
	int result = 0;

	checkParams(argc, argv);
	if (argc < 2) {
		printHelp();
		return -1;
	}

	printf("connecting to %s ...\n", serverAddr);
	result = arvid_client_connect(serverAddr);
	if (result < 0) {
		return 1;
	}
	printf("connected!\n");

	result = readFile(argv[1], &data);
	if (result < 1) {
		closeArvid();
		printf("failed to read file: %s\n", argv[1]);
		return -1;
	}
	printf("uploading %i bytes\n", result);
	
	printf("Press and hold Coin button to start...\n");

	if (waitForCoin() != 0) {
		printf("timed out.\n");
		closeArvid();
		return -1;
	}

	result = arvid_client_update_server(data, result);
	if (result != 0) {
		printf("failed to upload file: '%s' \n", argv[1]);
		closeArvid();
		return -1;
	}
	printf("Done. Now power-cycle Arvid.\n");
	return 0;
}

