#include <stdlib.h>
#include <stdlib.h>

#include "tsync.h"

/* returns 0 if OK */
int tsync_mutex_open (tsync_mutex* mutex) {
	*mutex = CreateEvent(NULL, FALSE, FALSE, NULL);
	return *mutex == NULL ? 1 : 0;
}

void tsync_mutex_close(tsync_mutex* mutex) {
	if (*mutex == NULL) {
		return;
	}
	CloseHandle(*mutex);
	*mutex = NULL;
}

void tsync_mutex_signal(tsync_mutex* mutex) {
	SetEvent(*mutex);
}

void tsync_mutex_wait(tsync_mutex* mutex) {
	WaitForSingleObject(*mutex, INFINITE);
}

void tsync_thread_start(tsync_thread* thread, void*(*func)(void*), void* data) {
	*thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) func, data, 0, NULL);
}
/* get the number of active CPU cores */
int tsync_get_cpu_cores(void) {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}


/* schedules thread on particular processor */
void tsync_thread_set_cpu(int cpuIndex) {
	//ignore - Windows seems to schedule threads fine and spreads the CPU load equally
}

