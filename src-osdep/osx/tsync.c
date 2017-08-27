#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <mach/thread_policy.h>

#include "tsync.h"

static int osxCoreHack = 0;

int tsync_mutex_open (tsync_mutex* mutex) {
	return pthread_mutex_init(mutex, NULL);
}

void tsync_mutex_close(tsync_mutex* mutex) {
	if (mutex == NULL) {
		return;
	}
	pthread_mutex_destroy(mutex);
}

void tsync_mutex_signal(tsync_mutex* mutex) {
	pthread_mutex_unlock(mutex);
}

void tsync_mutex_wait(tsync_mutex* mutex) {
	pthread_mutex_lock(mutex);
}

void tsync_thread_start(tsync_thread* thread, void*(*func)(void*), void* data) {
	pthread_create(thread, NULL, (void*) func, data);
}

/* get the number of active CPU cores */
int tsync_get_cpu_cores(void) {
	int result = sysconf(_SC_NPROCESSORS_ONLN);
	//Use at least 2 cores as it seems the arvid-client  and SDL doesn play well
	//if only 1 core is used.
	if (result < 2) {
	    osxCoreHack = 1;
	    result = 2;
	}
	return result;
}

/* schedules thread on particular processor */
void tsync_thread_set_cpu(int cpuIndex) {
    if (osxCoreHack) {
	return;
    }
    //credits: http://yyshen.github.io/2015/01/18/binding_threads_to_cores_osx.html
    int core = cpuIndex;
    tsync_thread thisThread = pthread_self();
    //printf("sched thread: %li to core %i\n", (long int) thisThread, cpuIndex);
    thread_affinity_policy_data_t policy = { core };
    thread_port_t mach_thread = pthread_mach_thread_np(thisThread);
    kern_return_t res = thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
    //printf("result=%li\n", (long int) res);

}

