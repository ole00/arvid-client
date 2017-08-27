#ifndef _TSYNC_H_
#define _TSYNC_H_

//Thread synchronisation

#include <pthread.h>
#include <semaphore.h>


typedef pthread_mutex_t tsync_mutex;

#define NULL_THREAD 0


/* create tsync mutex. returns 0 on success */
int tsync_mutex_open (tsync_mutex* mutex);

/* close tsync mutex */
void tsync_mutex_close(tsync_mutex* mutex);

/* signal tsync mutex */
void tsync_mutex_signal(tsync_mutex* mutex);

/* wait tsync mutex */
void tsync_mutex_wait(tsync_mutex* mutex);


typedef pthread_t tsync_thread;

/* creates and starts a thread */
void tsync_thread_start(tsync_thread* thread, void*(*func)(void*), void* data);

/* get the number of active CPU cores */
int tsync_get_cpu_cores(void);

/* schedules thread on particular processor */
void tsync_thread_set_cpu(int cpuIndex);


#endif
