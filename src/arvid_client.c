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

/* Arvid udp client - support multi-threaded frame buffer compression */


#ifdef MINGW
#include <winsock2.h>
#include <windows.h>
#define PAYLOAD_TYPE (const char *)
#define MSG_DONTWAIT 0
#define OS_DONTWAIT(v,s) {u_long mode=v; ioctlsocket(s,FIONBIO,&mode);}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#define PAYLOAD_TYPE
#define OS_DONTWAIT(v,s)
#endif

#include <stdio.h>
#include <memory.h>
#include "zlib.h"

#include "tsync.h"
#include "crc.h"
#include "arvid_client.h"

#define ARVID_CLIENT_VERSION "0.4f"


#define CMD_BLIT 1
#define CMD_FRAME_NUMBER 2
#define CMD_VSYNC 3
#define CMD_SET_VIDEO_MODE 4
#define CMD_GET_VIDEO_MODE_LINES 5
#define CMD_GET_VIDEO_MODE_FREQ 6
#define CMD_GET_WIDTH 7
#define CMD_GET_HEIGHT 8
#define CMD_ENUM_VIDEO_MODES 9 
#define CMD_INIT 11
#define CMD_CLOSE 12

#define CMD_GET_LINE_MOD 32
#define CMD_SET_LINE_MOD 33
#define CMD_SET_VIRT_VSYNC 34

#define CMD_UPDATE_START 40
#define CMD_UPDATE_PACKET 41
#define CMD_UPDATE_END 42

#define CMD_SERVER_POWEROFF 50



//number of compression tasks (including the main thread)
#define MAX_TASK 8

//big endian  conversion
#ifdef BIG_ENDIAND
#define SET_SHORT(x) ((unsigned short) (((x & 0xFF) << 8) | ((x & 0xFF00) >> 8)))
#define GET_INT(x) ((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3])
#define GET_SHORT(x) ((x[0] << 8) | x[1])
#else
#define SET_SHORT(x) ((unsigned short) (x & 0xFFFF));
#define GET_INT(x) (((int)x[3] << 24) | ((int)x[2] << 16) | ((int)x[1] << 8) | x[0])
#define GET_SHORT(x) (((int)x[1] << 8) | x[0])
#endif

#define VERBOSE (0)

#define PACKET_CNT 3
#define RESPONSE_SIZE 6

typedef struct arvid_client_data_t {
	int socketFd;
	struct sockaddr_in serverAddr;
	struct sockaddr_in clientAddr;
	unsigned short payload [1 * 1024 + 8];
	unsigned short recv[128]; //previously 12, now list of videomodes can be send back so it is larger
	unsigned long statSize;			//transferred size
	int width;
	int height;
	int cpuCores;					//total number of cores to use
	int blitType;
	char opened;
	char blitWait;
	int buttons;				//button status
} arvid_client_data;

//data passed to compression threads
typedef struct arvid_client_task_t {
	tsync_thread thread;
	tsync_mutex mutexStart;	//start of the job
	tsync_mutex mutexEnd;	//finished the job
	z_stream zStream;
	unsigned short* buffer;		//frame buffer start (source data)
	unsigned short payload [16 * 1024 + 8];	//destination (compressed) data
	int yPos;					//initial line
	int width;
	int height;					//lines to transfer
	int stride;					//stride of a single line
	int taskIndex;
	volatile char started;
	volatile char stopped;
} arvid_client_task;

arvid_client_data ac;
arvid_client_task at[MAX_TASK]; 

static unsigned short sendId = 0;
static unsigned short recvId = 0;

static void sleep_(int ms) {
	#ifdef MINGW
	Sleep(ms);
	#else
	usleep(ms * 1000);
	#endif
	
}


static int receiveResult_(int dataSize ) {
	unsigned short id = recvId;
	unsigned char* data = (unsigned char*) ac.recv;
//	printf("recv..\n");
	//receive result from the server
	while(1) {
	    int size = recvfrom(ac.socketFd, data, dataSize, 0, NULL,NULL);
	    id = (unsigned short) GET_SHORT(data);
	    //ignore packets with the same id
	    if (id != recvId) {
		break;
	    }
	}
	recvId = id;
	data += 2; //skip the id
	return (int) GET_INT(data);
}
static int receiveResultNonBlocking_(void) {
	unsigned char* data = (unsigned char*) ac.recv;
//	printf("recv..\n");
	//receive result from the server

	//set non-blocking mode
	OS_DONTWAIT(1, ac.socketFd);
	int size = recvfrom(ac.socketFd, data, 6, MSG_DONTWAIT, NULL,NULL);

	//set blocking mode
	OS_DONTWAIT(0, ac.socketFd);
	if (size >= 0) {
	    recvId = (unsigned short) GET_SHORT(data);
	}
	return size;
}

static int sendCommand_(int size) {
	int i;
	int result = 0;
	ac.payload[1] = ++sendId;
	size++;

	for (i = 0; i < PACKET_CNT; i++) {
		result = sendto(ac.socketFd,  PAYLOAD_TYPE ac.payload, 2 * size, 0,
			(struct sockaddr *)& ac.serverAddr, sizeof(ac.serverAddr));
	}

	return result;
}
static void sendCommandWithPayload_(unsigned short* payload, int size) {
	sendto(ac.socketFd, PAYLOAD_TYPE payload, 2 * size, 0,
		(struct sockaddr *)& ac.serverAddr, sizeof(ac.serverAddr));
}

// This function can run as a thread loop
// or can be called directly from the main thread.
// When run in separate thread it waits for the 
// signal to start the job and when the job is finished
// it signals the job is done. This is repeated indefinitely.
// When it runs in main thread it just does one iteration
// and then exists back without waiting or signalling.
// The job of this runner is to compress portion of the screen
// buffer and then send the compressed data to Arvid server. 
static void* threadRunner_(void* data) {
	int ret;
	int lines;
	int size;

	arvid_client_task* td = (arvid_client_task*) data;


	if (td->taskIndex > 0 ) {
		if (VERBOSE) {
			printf("arvid client: task %i started! %p\n", td->taskIndex, td);
		}
		tsync_thread_set_cpu(td->taskIndex);
	}
	td->started = 100 + td->taskIndex;
	td->payload[0] = 1; // command blit buffer

	//initialise z stream
	//raw buffer, level 2 (seems to be the quick enough)
	ret = deflateInit2(&td->zStream, 2, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		printf("arvid_client: deflate init failed\n");
		return NULL;
	}
	
	while(1) {
		
		//wait for the job start signal if running in a thread
		if (td->taskIndex > 0) {
			//printf("arvid client: task wait! %p\n", td);
			tsync_mutex_wait(&td->mutexStart);
			//printf("arvid client: task signalled! %p\n", td);
			
			if (td->stopped) {
			    break;
			}
		}
		
		//compress the frame buffer
		{
			int y;
			unsigned short* buffer = td->buffer;
			int compressedSize;
			unsigned char* pix;
			int posY = td->yPos;
			int block = 32;
			int chunkSize = (block << 11); // (block * 1024) * 2
			//printf("stride=%i\n", td->stride);
			//max size of single chunk is 32 Kbytes
			if (td->stride > 512) {
				block >>= 1;
				chunkSize >>= 1;
			}
			//send block of 32 or 16 lines at a time
			for (y = 0; y < td->height; y += block) {
				lines = td->height - y;
				if (lines > block) {
					lines = block;
				}
				
				pix = (unsigned char*) &td->payload[8];

				size = lines * td->stride; 
				compressedSize = 0;
				//source
				td->zStream.next_in = (void *) buffer;
				td->zStream.avail_in = (size << 1);
				//destination
				td->zStream.next_out = (void *)pix;
				td->zStream.avail_out = chunkSize;
				
				ret = deflate(&td->zStream, Z_FINISH);
				compressedSize = chunkSize - td->zStream.avail_out;
				deflateReset(&td->zStream);
				//printf("y: %i deflate: %i stride: %i src_size: %i buf: %p\n", y, compressedSize, td->stride, size << 1, buffer );
				buffer += size;

				td->payload[1] = SET_SHORT(compressedSize);
				td->payload[2] = SET_SHORT(posY);
				td->payload[3] = 0;
				//send the data
				sendto(ac.socketFd, PAYLOAD_TYPE td->payload, (8  << 1) + compressedSize, 0,
					(struct sockaddr *)& ac.serverAddr, sizeof(ac.serverAddr));
				
				//ac.statSize += (8 << 1) + compressedSize;
				posY += block;

			} //end for
		}
		//signal the job has finished 
		if (td->taskIndex > 0) {
			tsync_mutex_signal(&td->mutexEnd);
		} else {
			break; //break from the while(1) loop
		}
	}
	deflateEnd(&td->zStream);
	if (td->taskIndex > 0) {
		if (VERBOSE) {
			printf("arvid_client: task %i finished\n", td->taskIndex);
		}
	}
	
	return NULL;
}

// prepare mutexes and start threads
static int createTasks_(void) {
	int i,j;

	//Index 0 will be run from the main thread.
	at[0].thread = NULL_THREAD;
	//set up this thread on cpu 0
	tsync_thread_set_cpu(0);

	for (i = 0; i < ac.cpuCores; i++) {
		//initialise zlib structures
		at[i].zStream.zalloc = Z_NULL;
		at[i].zStream.zfree = Z_NULL;
		at[i].zStream.opaque = Z_NULL;
		at[i].taskIndex = i;
		at[i].started = 0;
		at[i].stopped = 0;
	}
	
	//Start from index 1. The index 0 is 
	//reserved for the main thread.
	for (i = 1; i < ac.cpuCores; i++) {
		//create mutex objects
		int result1 = tsync_mutex_open(&at[i].mutexStart);
		int result2 = tsync_mutex_open(&at[i].mutexEnd);

		//sanity check - mutex are OK
		if (result1 != 0  || result2 != 0) {
			printf("arvid_client error: mutex open failed for task %i !\n", i);
			return 2;
		} else {
			//start the compression thread
			tsync_thread_start(&at[i].thread, threadRunner_, &at[i]);
			if (at[i].thread == NULL_THREAD) {
			    printf("arvid_client error: thread start failed for task %i !\n", i);
			}
		}
	}
	
	//wait to make sure the threads are started
	for (j = 0; j < 10; j++) {
		int total = 1;
		sleep_(100);
		for (i = 1; i < ac.cpuCores; i++) {
		    if (at[i].started == (100 + i)) {
			total++;
		    }
		}
		if (total == ac.cpuCores) {
			if (VERBOSE) {
				printf("arvid_client: all tasks started! total=%i \n", total);
			}
			return 0;
		}
	}
	return 1;
}

static void initSockets_(void) {
	#ifdef MINGW
	WORD version = 0x0202;
	WSADATA wsaData;
	int res = WSAStartup(version, &wsaData);
	if (res != 0) {
		printf("failed to init winsock2 res=%i\n", res);
	}
	#endif
}

static void disposeSockets_(void)  {
	#ifdef MINGW
	WSACleanup();
	#endif
}


int arvid_client_connect(char* serverAddress) {

	memset(&ac, 0, sizeof(ac));

	ac.cpuCores = tsync_get_cpu_cores();
	if (ac.cpuCores < 1 || ac.cpuCores > MAX_TASK) {
		ac.cpuCores = MAX_TASK;
	}
	
	printf("arvid_client: ver. " ARVID_CLIENT_VERSION " - multithreaded (cores: %i)\n", ac.cpuCores);
	//prepare task data and start threads;
	if (createTasks_() != 0) {
		printf("arvid_client: failed to create tasks!\n");
		return -101;
	}

	printf("arvid_client: connecting to '%s'\n", serverAddress);

	initSockets_();
	ac.socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	ac.serverAddr.sin_family = AF_INET;
	ac.serverAddr.sin_addr.s_addr = inet_addr(serverAddress);
	ac.serverAddr.sin_port = htons(32100);
	ac.statSize = 0;
	ac.width = 0;
	ac.height = 0;
	ac.blitWait = 0;
	ac.blitType = ARVID_BLIT_TYPE_BLOCKING;

	if (ac.socketFd >= 0) {
		int result = -101;
		ac.payload[0] = CMD_INIT;
		sendCommand_(1);
		//wait a bit (1 sec) to ensure the response got back
		sleep_(1000);
		result = receiveResultNonBlocking_();
		if (result < 0) {
			printf("arvid_client: connection failed!\n");
			disposeSockets_();
		} else {
		    ac.opened = 1;
		}
		return result;
	} else {
		printf("arvid_client: failed to create socket. ret=%i\n", ac.socketFd);
	}

	disposeSockets_();
	return -100;
}



//send the buffer to arvid hidden frame-buffer
int arvid_client_blit_buffer(unsigned short* buffer, int width, int height,  int stride) {
	int i;
	int yPos;
	int taskCount;
	int linesPerTask;
	int taskStart;
	int taskEnd;
	
	if (!ac.opened) {
	    return -1;
	}
	
	taskCount = ac.cpuCores;
	taskStart = 0;
	taskEnd = taskCount;

	if (ac.blitType == ARVID_BLIT_TYPE_NON_BLOCKING) {
		taskStart = 1;
		taskCount -= 1;  
	}
	linesPerTask = ((height / taskCount) >> 2) << 2; //multiplies of 4 lines
	
	if (taskCount < 2)  {
		linesPerTask = height;
	} else
	if (linesPerTask * taskCount < height) {
		linesPerTask += 4;
	}
	
	yPos = 0;
	//distribute task data
	for (i = taskStart; i < taskEnd; i++) {
		int lines = height - yPos;
		if (lines > linesPerTask) {
			lines = linesPerTask;
		}
		at[i].buffer = buffer;
		at[i].yPos = yPos;
		at[i].height = lines;
		at[i].width = width;
		at[i].stride = stride;
		
		yPos += lines;
		//each task will get different portion of the screen buffer
		//assigned to compress and to transfer
		buffer += (lines * stride);

		//signal start of the task (the task should be waiting locked on its start mutex)
		if (i > 0) {
			tsync_mutex_signal(&at[i].mutexStart);
		}
	}

	//NON_BLOCKING
	if (ac.blitType == ARVID_BLIT_TYPE_NON_BLOCKING) {
		ac.blitWait = 1;
	} else  
	//BLOCKING
	{
		//run the first task in this thread
		threadRunner_(&at[0]);
	
		//now wait till all remaining tasks have finished
		for (i = 1; i < taskCount; i++) {
			tsync_mutex_wait(&at[i].mutexEnd);
		}
	}
	
	
	return 0;
}

unsigned int arvid_client_get_stat_transferred_size(void) {
	unsigned int result = ac.statSize;
	ac.statSize = 0;
	return result;
}

unsigned int arvid_client_get_frame_number(void) {
	if (!ac.opened) {
	    return 0;
	}

	ac.payload[0] = CMD_FRAME_NUMBER; //get frame number
	sendCommand_(1);
	return (unsigned int) receiveResult_(RESPONSE_SIZE);
}

unsigned int arvid_client_wait_for_vsync(void) {
	int taskEnd = ac.cpuCores;
	int i;
	int result;

	if (!ac.opened) {
	    return 0;
	}

	//blitWait is only set in NON_BLOCKING blit
	if (ac.blitWait) {
		//now wait till all remaining rendering tasks have finished
		for (i = 1; i < taskEnd; i++) {
			tsync_mutex_wait(&at[i].mutexEnd);
		}
	}
	ac.blitWait = 0;

	ac.payload[0] = CMD_VSYNC; //wait for vsync
	sendCommand_(1);
	result = receiveResult_(10); //read 10 bytes

	//store button status
	{
		unsigned char* data = (unsigned char*) ac.recv;
		data += 6; //button status at index 6
		ac.buttons = GET_INT(data);
	}
	return (unsigned int) result; 
}

int arvid_client_get_button_status(void) {
	if (!ac.opened) {
	    return -1;
	}
	return ac.buttons;
}

int arvid_client_set_video_mode(int mode, int lines) {	
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_SET_VIDEO_MODE; //set video mode
	ac.payload[2] = SET_SHORT(mode);
	ac.payload[3] = SET_SHORT(lines);
	ac.width = 0;
	ac.height = 0;
	sendCommand_(3);
	return receiveResult_(RESPONSE_SIZE);
}

int arvid_client_get_video_mode_lines(int mode, float frequency) {
	int frq = frequency * 1000;
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_GET_VIDEO_MODE_LINES; //get video mode lines
	ac.payload[2] = SET_SHORT(mode);
	ac.payload[3] = SET_SHORT(frq);
	sendCommand_(3);
	ac.height = receiveResult_(RESPONSE_SIZE);
	printf("arvid_client: get video mode lines. mode=%i freq=%f lines=%i\n", mode, frequency, ac.height);
	return ac.height;
}


float arvid_client_get_video_mode_refresh_rate(int mode, int lines) {
	if (!ac.opened) {
	    return 0;
	}
	
	ac.payload[0] = CMD_GET_VIDEO_MODE_FREQ; //get video mode refresh rate
	ac.payload[2] = SET_SHORT(mode);
	ac.payload[3] = SET_SHORT(lines);

	sendCommand_(3);
	return (float) (receiveResult_(RESPONSE_SIZE) / 1000.0f);
}

int arvid_client_get_width(void) {
	if (ac.width > 0) {
		return ac.width;
	}
	if (!ac.opened) {
	    return 0;
	}
	ac.payload[0] = CMD_GET_WIDTH; //get width
	sendCommand_(1);
	ac.width = receiveResult_(RESPONSE_SIZE);
	return ac.width;
}

int arvid_client_get_height(void) {
	if (ac.height > 0) {
		return ac.height;
	}
	if (!ac.opened) {
	    return 0;
	}
	ac.payload[0] = CMD_GET_HEIGHT; //get height
	sendCommand_(1);
	ac.height = receiveResult_(RESPONSE_SIZE);
	return ac.height;
}

int arvid_client_enum_video_modes(arvid_client_vmode_info* vmodes, int maxItem) {
	int result;
	unsigned short* vdata = &ac.recv[3]; 
	if (!ac.opened) {
	    return -1;
	}	
	if (vmodes == NULL || maxItem < 1) {
		return -1;
	}
	ac.payload[0] = CMD_ENUM_VIDEO_MODES;
	sendCommand_(1);
	result = receiveResult_(RESPONSE_SIZE + 120);
	printf("arvid: enum video modes result=%i\n", result);
	if (result > 0 && maxItem >= result) {
		memcpy(vmodes, vdata, sizeof(arvid_client_vmode_info) * result);
		return result;
	}
	return -1;
}

int arvid_client_close(void) {
	int result;
	int i;

	if (!ac.opened) {
	    return -1;
	}

	for (i = 1; i < ac.cpuCores; i++) {
		at[i].stopped = 1;
		tsync_mutex_signal(&at[i].mutexStart); //wake up the task
	}
	
	ac.payload[0] = CMD_CLOSE; //close
	sendCommand_(1);
	result = receiveResult_(RESPONSE_SIZE);

	close(ac.socketFd);
	ac.socketFd = -1;
	ac.height = 0;
	ac.width = 0;
	ac.opened = 0;
	sendId = 0;
	recvId = 0;
	//let the threads to finish
	sleep_(100);
	//destroy mutexes
	for (i = 1; i < ac.cpuCores; i++) {
		tsync_mutex_close(&at[i].mutexStart);
		tsync_mutex_close(&at[i].mutexEnd);
	}
	disposeSockets_();
	return result;
}

int arvid_client_set_blit_type(int type) {
	if (!ac.opened) {
	    return -1;
	}
	//check invalid blocking type
	if (!(type == ARVID_BLIT_TYPE_BLOCKING || type == ARVID_BLIT_TYPE_NON_BLOCKING)) {
	    return -2;
	}

	if (ac.cpuCores <= 1 && type == ARVID_BLIT_TYPE_NON_BLOCKING) {
		ac.blitType = ARVID_BLIT_TYPE_BLOCKING;
		return -3;
	}

	ac.blitType = type;
	return 0;
}

int arvid_client_set_virtual_vsync(int vsyncLine) {
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_SET_VIRT_VSYNC; //set virtual vsync line or disable
	//payload[1] is reserverd (contains packet id)
	ac.payload[2] = SET_SHORT(vsyncLine );
	sendCommand_(2);
	return 0;
}

int arvid_client_set_line_pos_mod(short mod) {
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_SET_LINE_MOD; //set line sync mod 
	//payload[1] is reserverd (contains packet id)
	ac.payload[2] = SET_SHORT(mod);
	sendCommand_(2);
	return 0;
}


int arvid_client_get_line_pos_mod(void) {
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_GET_LINE_MOD; //get line position modifier
	sendCommand_(1);
	return receiveResult_(RESPONSE_SIZE);
}

int arvid_client_power_off_server(void) {
	if (!ac.opened) {
	    return -1;
	}
	ac.payload[0] = CMD_SERVER_POWEROFF;
	sendCommand_(1);
	return receiveResult_(RESPONSE_SIZE);
}

/* sends an update file to the server */
int arvid_client_update_server(unsigned char* updateData, int updateSize) {
	int result = 0;
	int index;
	int size = updateSize;
	unsigned char* data = updateData;
	unsigned int crc;
	if (!ac.opened) {
	    return -1;
	}
	if (updateData == NULL || updateSize < 1) {
		return -2;
	}
	ac.payload[0] = CMD_UPDATE_START;
	//payload[1] is reserverd (contains packet id)
	ac.payload[2] = SET_SHORT((updateSize & 0xFFFF));
	ac.payload[3] = SET_SHORT((updateSize >> 16));
	
	sendCommand_(3);
	result = receiveResult_(RESPONSE_SIZE);
	if (result != 0) {
		printf("Error: failed to upload update file. %i\n", result);
		return result;
	}
	
	index = 0;
	while (updateSize > 0) {
		int  block = (updateSize > 1024) ? 1024 : updateSize;
		ac.payload[0] = CMD_UPDATE_PACKET;
		//payload[1] is reserverd (contains packet id)
		ac.payload[2] = SET_SHORT(index);
		ac.payload[3] = SET_SHORT(block);
		memcpy(&ac.payload[4], updateData, block);
		sendCommand_(4 + ((block + 1) / 2));
		updateData += block;
		updateSize -= block;
		index++;
	}
	
	crc = crc_calc(data, size);
	printf("crc=0x%08x\n", crc);
	ac.payload[0] = CMD_UPDATE_END;
	//payload[1] is reserverd (contains packet id)
	ac.payload[2] = SET_SHORT((crc & 0xFFFF));
	ac.payload[3] = SET_SHORT((crc >> 16));
	
	sendCommand_(3);
	result = receiveResult_(RESPONSE_SIZE);
	if (result != 0) {
		printf("Error: failed to send update file. %i\n", result);
		return 4;
	}
	
	return 0;
}
