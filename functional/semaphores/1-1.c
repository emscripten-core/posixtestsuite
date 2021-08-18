/*
 * Author: HongJunxin
 * 
 * Test: Semaphore schedule policy based on priority
 *
 * Test step:
 * 1. Main initilizes semaphore with value 0
 * 2. Main create thread A with low prioriy. A execute sem_wait.
 * 3. Main create thread B with middle priority. B execute sem_wait.
 * 4. Main create thread C with high priority. C execute sem_wait.
 * 5. Main execute sem_post.
 * 6. Thread C go ahead from sem_wait and then do sem_post.
 * 7. Thread B go ahead from sem_wait and then do sem_post.
 * 8. Thread A go ahead from sem_wait and then do sem_post.
 *
 */
 
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "posixtest.h" 
 
#define THREAD_ENTER	0
#define THREAD_EXIT		1

#define SCHED_POLICY SCHED_RR  /* or SCHED_FIFO */

static int thread_a_status;
static int thread_b_status;
static int thread_c_status;
static void create_thread(pthread_t *tid, int priority, void *(*fn)(void *));
static int priority;
 
static sem_t sem;

static void *fn_a(void *arg);
static void *fn_b(void *arg);
static void *fn_c(void *arg);

static char* get_thread_status(int no);
 
int main(int argc, char **argv)
{	
	 pthread_t thread_a, thread_b, thread_c;
	 
	 priority = sched_get_priority_min(SCHED_POLICY);
	 
	 if (sem_init(&sem, 0, 0) != 0) {
		 printf("Test FAILED. Error at sem_init()\n");
		 exit(PTS_FAIL);
	 }
	 
	 printf("[Main] create thread A with priority %d\n", priority);
	 create_thread(&thread_a, priority, fn_a);
	 
	 priority += 2;
	 printf("[Main] create thread B with priority %d\n", priority);
	 create_thread(&thread_b, priority, fn_b);
	 
	 priority += 2;
	 printf("[Main] create thread C with priority %d\n", priority);
	 create_thread(&thread_c, priority, fn_c);	 
	 
	 sleep(1);  /* To make sure all child thread block on sem_wait() */
	 
	 printf("[Main] sem_post\n");
	 if (sem_post(&sem) != 0) {
		 printf("Test FAILED. Error at sem_post()\n");
		 exit(PTS_FAIL);			 
	 }
	 
	 if (pthread_join(thread_a, NULL) != 0) {
		 printf("Test FAILED. Error at pthread_join() a\n");
		 exit(PTS_FAIL);		 
	 }
	 
	 if (pthread_join(thread_b, NULL) != 0) {
		 printf("Test FAILED. Error at pthread_join() b\n");
		 exit(PTS_FAIL);		 
	 }

	 if (pthread_join(thread_c, NULL) != 0) {
		 printf("Test FAILED. Error at pthread_join() c\n");
		 exit(PTS_FAIL);		 
	 }
	 
	 if (sem_destroy(&sem) != 0) {
		 printf("Test FAILED. Error at sem_destroy()\n");
		 exit(PTS_FAIL);			 
	 }
	 
	 printf("Test PASSED\n");
	 return PTS_PASS;
}

static void *fn_a(void *arg)
{
	thread_a_status = THREAD_ENTER;	
	
	printf("[Thread A] sem_wait\n");
	if (sem_wait(&sem) != 0) {
		printf("Test FAILED. Error at sem_wait() a\n");
		exit(PTS_FAIL);			
	}
	
	if (thread_b_status != THREAD_EXIT || 
			thread_c_status != THREAD_EXIT) {
		printf("Test FAILED. Thread B and C should run done before A\n");
		exit(PTS_FAIL);
	}
	
	thread_a_status = THREAD_EXIT;
	printf("[Thread A] exit\n");
}

static void *fn_b(void *arg)
{
	thread_b_status = THREAD_ENTER;
	
	printf("[Thread B] sem_wait\n");
	if (sem_wait(&sem) != 0) {
		printf("Test FAILED. Error at sem_wait() b\n");
		exit(PTS_FAIL);			
	}
	
	if (thread_c_status != THREAD_EXIT || 
			thread_a_status != THREAD_ENTER) {
		printf("Test FAILED. Thread C should run done and thread A should not exit\n");
		printf("[Thread B] thread A status=%s, thread C status=%s\n",
				get_thread_status(thread_a_status), get_thread_status(thread_c_status));
		exit(PTS_FAIL);
	}
	
	thread_b_status = THREAD_EXIT;	
	if (sem_post(&sem) != 0) {
		printf("Test FAILED. Error at sem_post() b\n");
		exit(PTS_FAIL);			
	}
	printf("[Thread B] sem_post and exit\n");
}

static void *fn_c(void *arg)
{
	thread_c_status = THREAD_ENTER;
	
	printf("[Thread C] sem_wait\n");
	if (sem_wait(&sem) != 0) {
		printf("Test FAILED. Error at sem_wait() c\n");
		exit(PTS_FAIL);			
	}
	
	if (thread_b_status == THREAD_EXIT || 
			thread_a_status == THREAD_EXIT) {
		printf("Test FAILED. Thread A and B should run done after C\n");
		exit(PTS_FAIL);
	}
	
	thread_c_status = THREAD_EXIT;	
	if (sem_post(&sem) != 0) {
		printf("Test FAILED. Error at sem_post() c\n");
		exit(PTS_FAIL);			
	}
	printf("[Thread C] sem_post and exit\n");
}

static char *get_thread_status(int no)
{
	if (no == THREAD_ENTER) 
		return "THREAD_ENTER";
	else if (no == THREAD_EXIT)
		return "THREAD_EXIT";
	else
		return "?Unknown?";		
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
    
    if (pthread_attr_setschedpolicy(&attr, SCHED_POLICY) != 0) {
        printf("Test FAILED. Error at pthread_attr_setschedpolicy()\n");
        exit(PTS_FAIL);        
    }
    
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0) {
        printf("Test FAILED. Error at pthread_attr_setinheritsched()\n");
        exit(PTS_UNRESOLVED);
    }    
    
    param.sched_priority = priority;
    if (pthread_attr_setschedparam(&attr, &param) != 0) {
        printf("Test FAILED. Error at pthread_attr_setschedparam(), errno=%d\n", errno);
        exit(PTS_FAIL);                
    }        

    ret = pthread_create(tid, &attr, fn, NULL);
    if (ret != 0) {
        if (ret == EPERM) {
            printf("Test FAILED. Permission Denied when creating"
                   " thread with policy %d\n", SCHED_POLICY);
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