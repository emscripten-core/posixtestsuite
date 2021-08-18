/*   
 * Copyright (c) 2004, Intel Corporation. All rights reserved.
 * This file is licensed under the GPL license.  For the full content
 * of this license, see the COPYING file at the top level of this 
 * source tree.
 * adam.li@intel.com
 *
 * The pthread_getschedparam( ) function shall retrieve the scheduling 
 * policy and scheduling parameters for the thread whose thread ID is 
 * given by thread and shall store those values in
 * policy and param, respectively. The priority value returned from 
 * pthread_getschedparam( ) shall be
 * the value specified by the most recent pthread_setschedparam( ), 
 * pthread_setschedprio( ), or pthread_create( ) call affecting the 
 * target thread. It shall not reflect any temporary adjustments to
 * its priority as a result of any priority inheritance or ceiling functions. 
 * 
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "posixtest.h"

void *a_thread_func(void* arg)
{
	struct sched_param sparam;
	int policy;
	int rc;
	
	rc = pthread_getschedparam(pthread_self(), &policy, &sparam);
	if (rc != 0)
	{
		printf("Error at pthread_getschedparam: rc=%d\n", rc);
		exit(PTS_FAIL);
	}
	printf("policy: %d, priority: %d\n", policy, sparam.sched_priority);	
	pthread_exit(0);
	return NULL;
}

int main()
{
	pthread_t new_th;
	
	if(pthread_create(&new_th, NULL, a_thread_func, NULL) != 0)
	{	
		perror("Error creating thread\n");
		return PTS_UNRESOLVED;
	}
	
	pthread_join(new_th, NULL);
	printf("Test PASSED\n");
	return PTS_PASS;	
}


