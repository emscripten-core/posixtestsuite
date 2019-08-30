/*   
 * Copyright (c) 2019, HongJunxin. All rights reserved.
 * Created by:  HongJunxin
 * This file is licensed under the GPL license.  For the full content
 * of this license, see the COPYING file at the top level of this 
 * source tree.

 This program tests pthread_rwlockattr_setkind_np() function.

 Using PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP to initialize rwlock
 which prefers to writer. Such rwlock can make sure writer thread has 
 chance to run in multi-reader scenario.

 Steps:
 1. Main thread initializes rwlock with PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP.
 2. Main thread creates reader thread 1.
 3. Main thread creates writer thread.
 4. Main thread creates reader thread 2.
 5. Reader thread 1 got rwlock and then waiting for reader thread 2 to
    tell him go ahead.
 6. Writer thread requires rwlock and then block on it.
 7. Reader thread 2 trys to require rwlock and it fails which is what we expect.
    Reader thread 2 tells reader thread 1 to go ahead. Reader thread 2 requires
	rwlock and block on it.
 8. Reader thread 1 go ahead and unlocks rwlock.
 9. Writer thread should get rwlock. Writer thread unlocks rwlock.
 10. Reader thread 2 gets rwlock.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include "posixtest.h"


#define FALSE 	0
#define TRUE 	1

static pthread_rwlock_t rwlock;
static int rd_1_thread_state;
static int rd_2_thread_state;
static int wr_thread_state; 
static int rd_1_pause;

/* thread states: 
	1: not in child thread yet; 
	2: just enter child thread ;
	3: just before child thread exit;
*/

#define NOT_CREATED_THREAD 1
#define ENTERED_THREAD 2
#define EXITING_THREAD 3


static void *fn_wr(void *arg)
{
	wr_thread_state = ENTERED_THREAD;
	
	printf("[writer thread] require rwlock\n");
	if (pthread_rwlock_wrlock(&rwlock) != 0) {
		printf("Error at writer thread pthread_rwlock_wrlock()\n");
		exit(PTS_UNRESOLVED);			
	}
	
	printf("[writer thread] got rwlock\n");
	if (rd_2_thread_state == EXITING_THREAD) {
		printf("Test FAILED: reader thread 1 has exited when reader thread got rwlock\n");
		exit(PTS_FAIL);
	}
	
	if (pthread_rwlock_unlock(&rwlock) != 0) {
		printf("Error at writer thread pthread_rwlock_unlock()\n");
		exit(PTS_UNRESOLVED);		
	}
	
	wr_thread_state = EXITING_THREAD;
	printf("[writer thread] unlock rwlock\n");
}

/* Hold rwlock for a while, to check writer prefered */
static void *fn_rd_1(void *arg)
{
	rd_1_thread_state = ENTERED_THREAD;
	
	if (pthread_rwlock_rdlock(&rwlock) != 0) {
		printf("Error at read thead 1 pthread_rwlock_unlock()\n");
		exit(PTS_UNRESOLVED);		
	}
	printf("[reader thread 1] got rwlock\n");
	
	rd_1_pause = TRUE;
	while (rd_1_pause == TRUE); // Wait reader thread 2 to let it go.
	
	sleep(2); // To make sure reader thread 2 and writer thread are waiting for rwlock
	
	if (pthread_rwlock_unlock(&rwlock) != 0) {
		printf("Error at read thead 1 pthread_rwlock_unlock()\n");
		exit(PTS_UNRESOLVED);
	}
	printf("[reader thread 1] unlocked rwlock\n");
	
	rd_1_thread_state = EXITING_THREAD;
}

static void *fn_rd_2(void *arg)
{
	int ret;
	rd_2_thread_state = ENTERED_THREAD;
	
	printf("[reader thread 2] try to get rwlock\n");
	ret = pthread_rwlock_tryrdlock(&rwlock);
	if (ret == 0) {
		printf("Test FAILED: reader thread 2 got the rwlock, but"
				" writer thread is still waiting for it \n");
		exit(PTS_FAIL);
	} else if (ret == EBUSY) {
		printf("[reader thread 2] tryrdlock() return EBUSY which we expect.\n");
	} else {
		printf("Error at pthread_rwlock_tryrdlock()\n");
		exit(PTS_UNRESOLVED);
	}
	
	rd_1_pause = FALSE;
	
	printf("[reader thread 2] require rwlock\n");
	if (pthread_rwlock_rdlock(&rwlock) != 0) {
		printf("Error at reader thread 2 pthread_rwlock_rdlock()\n");
		exit(PTS_UNRESOLVED);
	}
	
	// double check 
	printf("[reader thread 2] got rwlock\n");
	if (wr_thread_state != EXITING_THREAD) {
		printf("Test FAILD: reader thread 2 got rwlock, but writer thread"
				" not exiting\n");
		exit(PTS_FAIL);
	}
	
	if (pthread_rwlock_unlock(&rwlock) != 0) {
		printf("Error at reader thread 2 pthread_rwlock_unlock()\n");
		exit(PTS_UNRESOLVED);
	}
	
	rd_2_thread_state = EXITING_THREAD;
	printf("[reader thread 2] unlock rwlock\n");	
}

static void pthread_join_fn(pthread_t t)
{
	if (pthread_join(t, NULL) != 0) {
		printf("Error at pthread_join()\n");
		exit(PTS_UNRESOLVED);
	}
}

int main(int argc, char **argv) 
{
	pthread_rwlockattr_t attr;	
	pthread_t rd_1_thread, rd_2_thread, wr_thread;

	if (pthread_rwlockattr_init(&attr) != 0) {
		printf("Error at pthread_rwlockattr_init()\n");
		return PTS_UNRESOLVED;		
	}
	
	/* PTHREAD_RWLOCK_PREFER_WRITER_NP not work for writer-prefered, but
       PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP can do it. */
	   
	pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
	
	if (pthread_rwlock_init(&rwlock, &attr) != 0) {
		printf("Error at pthread_rwlock_init()\n");
		return PTS_UNRESOLVED;
	} 
	
	if (pthread_rwlockattr_destroy(&attr) != 0) {
		printf("Error at pthread_rwlockattr_destroy()\n");
		return PTS_UNRESOLVED;		
	}
	
	printf("[main] create reader thread 1\n");
	if (pthread_create(&rd_1_thread, NULL, fn_rd_1, NULL) != 0) {
		printf("[main] Error at reader thread 1 pthread_create()\n");
		return PTS_UNRESOLVED;		
	}
	
	sleep(1); // Make sure reader thread 1 got rwlock
	
	printf("[main] create writer thread\n");	
	if (pthread_create(&wr_thread, NULL, fn_wr, NULL) != 0) {
		printf("[main] Error at writer thread pthread_create()\n");
		return PTS_UNRESOLVED;		
	}

	sleep(1); // Make sure writer thread is waitting for rwlock
		
	printf("[main] create reader thread 2\n");
	if (pthread_create(&rd_2_thread, NULL, fn_rd_2, NULL) != 0) {
		printf("[main] Error at reader thread 2 pthread_create()\n");
		return PTS_UNRESOLVED;		
	}	
	
	pthread_join_fn(rd_1_thread);
	pthread_join_fn(wr_thread);
	pthread_join_fn(rd_2_thread);
	
	printf("Test PASSED\n");
	return PTS_PASS;
}