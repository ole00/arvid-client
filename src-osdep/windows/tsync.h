#ifndef _TSYNC_H_
#define _TSYNC_H_

//Thread synchronisation

#include <windows.h>

#define NULL_THREAD NULL

typedef HANDLE tsync_mutex;


/* create tsync mutex. returns 0 on success */
int tsync_mutex_open (tsync_mutex* mutex);

/* close tsync mutex */
void tsync_mutex_close(tsync_mutex* mutex);

/* signal tsync mutex */
void tsync_mutex_signal(tsync_mutex* mutex);

/* wait tsync mutex */
void tsync_mutex_wait(tsync_mutex* mutex);


typedef HANDLE tsync_thread;

/* creates and starts a thread */
void tsync_thread_start(tsync_thread* thread, void*(*func)(void*), void* data);

/* schedules thread on particular processor */
void tsync_thread_set_cpu(int cpuIndex);

#endif
