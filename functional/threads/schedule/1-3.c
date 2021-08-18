/*
 * Created by:  HongJunxin
 * This file is licensed under the GPL license.  For the full content
 * of this license, see the COPYING file at the top level of this
 * source tree.
 * 
 * Test avoidance of prioriy reversal 
 *
 * Test steps:
 * 1. Main initializes mutex mtx with PTHREAD_PRIO_INHERIT property
 * 2. Main creates thread B using SCHED_FIFO, low priority.
 * 3. Main creates thread A using SCHED_FIFO, high priority.
 * 4. Main creates thread C using SCHED_FIFO, middle priority.
 * 5. Thread A B C run in the same cpu
 * 6. Thread B get mutex and then fall into while loop
 * 7. Thread A acquire mutex and block on it.
 * 8. Thread C is suspended waitting for thread B run done
 * 9. Thread B quit while loop and release mutex
 * 10. Thread A get mutex and then run done
 * 11. Thread C run done
 * 12. Thread B run done
 */


#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "posixtest.h"

#define TRD_POLICY 	SCHED_FIFO
#define RUNNING		0
#define RUN_DONE	1
#define FALSE    	0
#define TURE     	1

static pthread_mutex_t mtx;
static int is_thread_c_pause;
static int a_thread_status;
static int c_thread_status;

static int single_cpu = FALSE;

static int set_cpu_affinity(int cpu_no)
{
	int ret;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu_no, &set);	
	ret = sched_setaffinity((pid_t)0, sizeof(set), &set);
	if (ret != 0) {
		printf("Error at sched_setaffinity()\n");
		exit(PTS_UNRESOLVED);
	}
	
	return 0;
}

static void *fn_a(void *arg) 
{
	int s;

	a_thread_status = RUNNING;
	if (single_cpu == FALSE)
		set_cpu_affinity(1);
	
	printf("[Thread A] acquires mutex\n");
	s = pthread_mutex_lock(&mtx); 
	if (s != 0) {
		printf("[Thread A] failed to lock mutex, error code=%d\n", s);
		exit(PTS_UNRESOLVED);
	}
	
	s = pthread_mutex_unlock(&mtx);
	if (s != 0) {
		printf("[Thread A] failed to unlock mutex, error code=%d\n", s);
		exit(PTS_UNRESOLVED);
	}	
	
	a_thread_status = RUN_DONE;
	printf("[Thread A] run done\n");
}

static void *fn_b(void *arg)
{
	int s;
	
	if (single_cpu == FALSE)
		set_cpu_affinity(1);
	
	printf("[Thread B] acquires mutex\n");
	s = pthread_mutex_lock(&mtx); 
	if (s != 0) {
		printf("[Thread B] failed to lock mutex, error caode=%d\n", s);
		exit(PTS_UNRESOLVED);
	}
	
	while (is_thread_c_pause);
	   
	/* If the priority of thread B didn't be raised, thread C (with middle priority)
  	   run done already. Because the priority of thread B has been raised (higher than C)
	   and it is running, so in this time point thread C should be in suspended. */   
	if (c_thread_status == RUN_DONE) {
		printf("Test FAILED. Thread C shouldn't run done, because thread B"
			   " has higher priority than it now and still in running.\n");
		exit(PTS_FAIL);
	}

	printf("[Thread B] unlock mutex\n");
	s = pthread_mutex_unlock(&mtx);	
	if (s != 0) {
		printf("[Thread B] failed to unlock mutex, error code=%d\n", s);
		exit(PTS_UNRESOLVED);
	}
	printf("[Thread B] run done\n");	
}

static void *fn_c(void *arg) 
{
	int s;

	if (single_cpu == FALSE)
		set_cpu_affinity(1);

	c_thread_status = RUNNING;
	printf("[Thread C] running...\n");

	c_thread_status = RUN_DONE;
	printf("[Thread C] run done\n");
}

static void create_thread(pthread_t *tid, int priority, void *(*fn)(void *))
{
	struct sched_param param;
	pthread_attr_t attr;
	int ret;
	
	memset(&param, 0, sizeof(param));
	memset(&attr, 0, sizeof(attr));
	
	if (pthread_attr_init(&attr) != 0) {
		printf("Test FAILED. Error at pthread_attr_init()\n");
		exit(PTS_FAIL);
	}
	
	if (pthread_attr_setschedpolicy(&attr, TRD_POLICY) != 0) {
		printf("Test FAILED. Error at pthread_attr_setschedpolicy()\n");
		exit(PTS_FAIL);		
	}
	
	/* PTHREAD_EXPLICIT_SCHED need root. Default is PTHREAD_INHERIT_SCHED which 
	   make child thread inherit parent attribute. */
	if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0) {
		printf("Test FAILED. Error at pthread_attr_setinheritsched()\n");
		exit(PTS_UNRESOLVED);
	}	
	
	param.sched_priority = priority;
	if (pthread_attr_setschedparam(&attr, &param) != 0) {
		printf("Test FAILED. Error at pthread_attr_setschedpolicy()\n");
		exit(PTS_FAIL);				
	}		

	ret = pthread_create(tid, &attr, fn, NULL);
	if (ret != 0) {
		if (ret == EPERM) {
			printf("Test FAILED. Permission Denied when creating"
				   " thread with policy %d\n", TRD_POLICY);
			exit(PTS_FAIL);
		} else {
			printf("Test FAILED. Error at pthread_create()\n");
			exit(PTS_FAIL);
		}
	}	
	
	if (pthread_attr_destroy(&attr) != 0) {
		printf("Test FAILED. Error at pthread_attr_destroy()\n");
		exit(PTS_FAIL);				
	}		
}

static void init_mutex(pthread_mutex_t *mutex)
{
	int ret;
	pthread_mutexattr_t mta;
	
	ret = pthread_mutexattr_init(&mta);
	if (ret != 0) {
		printf("Error at pthread_mutexattr_init()\n");
		exit(PTS_FAIL);	
	}

	/* mutex protocol should be set to PTHREAD_PRIO_INHERIT */
	ret = pthread_mutexattr_setprotocol(&mta, PTHREAD_PRIO_INHERIT);
	if (ret != 0) {
		printf("Error at pthread_mutexattr_setprotocol()\n");
		exit(PTS_FAIL);	
	}
	
	ret = pthread_mutex_init(mutex, &mta);
	if (ret != 0) {
		printf("Error at pthread_mutex_init()\n");
		exit(PTS_FAIL);	
	}
	
	if(pthread_mutexattr_destroy(&mta) != 0) {
		printf("Error at pthread_mutexattr_destroy()\n");
		exit(PTS_FAIL);	
	}	
}

int main()
{		
	int priority;
	int policy;
	int cpu_num;
	pthread_t a_thread, b_thread, c_thread;    
    
    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    if (cpu_num == 1)
        single_cpu = TURE;
	
	init_mutex(&mtx);
	
	priority = sched_get_priority_min(TRD_POLICY);
	printf("[Main] create B thread, with priority: %d\n", priority);	
	is_thread_c_pause = 1;
	create_thread(&b_thread, priority, fn_b);	
	
	priority = sched_get_priority_min(TRD_POLICY) + 6;
	printf("[Main] create A thread, with priority: %d\n", priority);
	create_thread(&a_thread, priority, fn_a);
	
	priority = sched_get_priority_min(TRD_POLICY) + 2;
	printf("[Main] create C thread, with priority: %d\n", priority);
	create_thread(&c_thread, priority, fn_c);
	
	sleep(1); // Make sure all thread are running
	
	is_thread_c_pause = 0;
	
	if (pthread_join(b_thread, NULL) != 0) {
		printf("Error at pthread_join()\n");
		return PTS_UNRESOLVED;		
	}
	
	if (pthread_join(a_thread, NULL) != 0) {
		printf("Error at pthread_join()\n");
		return PTS_UNRESOLVED;		
	}
	
	if (pthread_join(c_thread, NULL) != 0) {
		printf("Error at pthread_join()\n");
		return PTS_UNRESOLVED;		
	}	

	printf("Test PASS.\n");
	return PTS_PASS;
}