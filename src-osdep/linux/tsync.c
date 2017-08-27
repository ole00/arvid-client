#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sched.h>

#include "tsync.h"


#define MAX_CORES 8

static int tsync_core_index = -1;
static int tsync_core_count = -1;
static int tsync_core_map[MAX_CORES] = {-1} ; //mapping for cores
static int tsync_core_map_enabled = 0;

int tsync_mutex_open (tsync_mutex* mutex) {
	return sem_init(mutex, 0, 0);
}

void tsync_mutex_close(tsync_mutex* mutex) {
	if (mutex == NULL) {
		return;
	}
	sem_destroy(mutex);
}

void tsync_mutex_signal(tsync_mutex* mutex) {
	sem_post(mutex);
}

void tsync_mutex_wait(tsync_mutex* mutex) {
	sem_wait(mutex);
}

void tsync_thread_start(tsync_thread* thread, void*(*func)(void*), void* data) {
	pthread_create(thread, NULL, (void*) func, data);
}

/* get the number of active CPU cores */
int tsync_get_cpu_cores(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN);
	
	//read maximum core count from environment variable
	if (tsync_core_count < 0) {
		char* envCoreCount = getenv("ARVID_CORE_COUNT");
		if (envCoreCount == NULL) {
			tsync_core_count = 256;
		} else {
			tsync_core_count = atoi(envCoreCount);
			printf("arvid_client: core limit = %i\n", tsync_core_count);
		}
	}

	if (tsync_core_count <  cores) {
		return tsync_core_count;
	}
	return cores;
}

static int _read_core_mapping(int core) {
	char envName[32];
	char* envCoreIndex;
	
	sprintf(envName, "ARVID_CORE_MAP_%i", core);
	envCoreIndex = getenv(envName);
	if (envCoreIndex == NULL) {
		return -1;
	} else {
		int coreIndex = atoi(envCoreIndex);
		printf("arvid_client: core %i mapped to %i\n", core, coreIndex);
		return coreIndex;
	}
}

/* schedules thread on particular processor */
void tsync_thread_set_cpu(int cpuIndex) {
	//schedule calling thread to be run on processor 1
	unsigned long mask;

	//read first core index from environment variable
	if (tsync_core_index < 0) {
		char* envCoreIndex = getenv("ARVID_CORE_INDEX");
		if (envCoreIndex == NULL) {
			tsync_core_index = 0;
		} else {
			tsync_core_index = atoi(envCoreIndex);
			printf("arvid_client: first core index = %i\n", tsync_core_index);
		}
		
		//try to read core mapping
		{
			int i;
			int cnt = 0;
			for (i = 0; i < MAX_CORES; i++) {
				tsync_core_map[i] = _read_core_mapping(i);
				if (tsync_core_map[i] >= 0 && tsync_core_map[i] < MAX_CORES) {
					cnt++;
				}
			}
			if (cnt == tsync_get_cpu_cores()) {
				tsync_core_map_enabled = 1;
				printf("arvid_client: core map enabled\n");
			}
			
		}
	}
	
	//remap core index according the map
	if (tsync_core_map_enabled) {
		cpuIndex = tsync_core_map[cpuIndex];
	} 
	else
	//use sequential index
	{
		cpuIndex += tsync_core_index;
	}

	mask = 1 << cpuIndex;
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
		printf("tsync_thread_set_cpu: failed to set thread affinity\n");
	}
}

