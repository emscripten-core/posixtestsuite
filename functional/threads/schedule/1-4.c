/*
 * Created by:  HongJunxin
 * This file is licensed under the GPL license.  For the full content
 * of this license, see the COPYING file at the top level of this
 * source tree.
 * 
 * Test SCHED_FIFO and SCHED_RR
 *
 * Test SCHED_FIFO if TRD_POLICY is set to SCHED_FIFO
 * Test SCHED_RR if TRD_POLICY is set to SCHED_RR
 *
 * Test steps:
 * Main create two thread with same priority and run in the same cpu.
 *
 * Expectation:
 * If schedule is SCHED_FIFO
 * Thread A should run done at first and then thread B begin running.
 *
 * If schedule id SCHED_RR
 * Thread A and B running rotational
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


#define TRD_POLICY SCHED_FIFO
#define TEST_COUNT  1
#define RUNNING     0
#define RUN_DONE    1

#define FALSE    0
#define TURE     1

static void create_thread(pthread_t *tid, int priority, void *(*fn)(void *));
static int set_cpu_affinity(int cpu_no);
static void *fn_a(void *arg);
static void *fn_b(void *arg);
static char *get_thread_status(int no);
static char *get_schedule(int no);

extern int errno;

static int task_a_status;
static int task_b_status;
static int single_cpu = FALSE;


int main(int argc, char **argv)
{
    pthread_t task_a, task_b;
    int i;
    int a_priority = 1;
    int b_priority = 1;    
    int cpu_num;
    
    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    if (cpu_num == 1)
        single_cpu = TURE;
    
    for (i=0; i<TEST_COUNT; i++) {        
        task_a_status = RUNNING;
        task_b_status = RUNNING;
        
        printf("[Main] create thread A with priority %d\n", a_priority);
        create_thread(&task_a, a_priority, fn_a);
        
        printf("[Main] create thread B with priority %d\n", b_priority);
        create_thread(&task_b, b_priority, fn_b);        


        pthread_join(task_a, NULL);
        pthread_join(task_b, NULL);
        
        printf("\n");
    }
    
    printf("Test PASSED\n");
    return PTS_PASS;
}


static void *fn_a(void *arg)
{
    if (single_cpu == FALSE)
        set_cpu_affinity(1);
    
    struct sched_param sparam;
    int policy;
    int rc;
    
    rc = pthread_getschedparam(pthread_self(), &policy, &sparam);
    if (rc != 0) {
        printf("Error at pthread_getschedparam: rc=%d\n", rc);
        exit(PTS_FAIL);
    }
    
    printf("[Thread A] running, policy=%s priority=%d\n",
            get_schedule(policy), sparam.sched_priority);

    unsigned int i;
    unsigned int a, b=100, c=100;

    for (i=0; i<4294967295; i++) {
        a = b*c-(b+c);
        a = a%4294967296;
        if (i%1000000000 == 0) {
            printf("[Thread A] running...\n");
        }
    }
    
    if (TRD_POLICY == SCHED_FIFO) {
        if (task_b_status == RUN_DONE) {
            printf("[Thread A] thread B shouldn't run done before me\n");
            printf("Test FAILED\n");
            exit(PTS_FAIL);
        }
    }
    
    task_a_status = RUN_DONE;
    printf("[Thread A] run done\n");
}


static void *fn_b(void *arg)
{
    if (single_cpu == FALSE)
        set_cpu_affinity(1);
    
    struct sched_param sparam;
    int policy;
    int rc;
    
    rc = pthread_getschedparam(pthread_self(), &policy, &sparam);
    if (rc != 0) {
        printf("Error at pthread_getschedparam: rc=%d\n", rc);
        exit(PTS_FAIL);
    }    
    
    printf("[Thread B] running, policy=%s priority=%d\n",
            get_schedule(policy), sparam.sched_priority);

    unsigned int i;
    unsigned int a, b=100, c=100;
    
    for (i=0; i<4294967295; i++) {
        a = b*c-(b+c);
        a = a%4294967296;
        if (i%1000000000 == 0) {
            printf("[Thread B] running...\n");
        }    
    }
    
    /* Double check here */
    if (TRD_POLICY == SCHED_FIFO) {
        if (task_a_status != RUN_DONE) {
            printf("[Thread B] thread A should run done now\n");
            printf("Test FAILED\n");
            exit(PTS_FAIL);
        }
    }
    
    task_b_status = RUN_DONE;
    printf("[Thread B] run done\n");
}


static char *get_thread_status(int no)
{
    if (no == RUNNING)
        return "Running";
    else if (no == RUN_DONE)
        return "Run Done";
    else
        return "?Unknown?";
}


static char *get_schedule(int no)
{
    if (no == SCHED_FIFO)
        return "FIFO";
    else if (no == SCHED_RR)
        return "RR";
    else if (no == SCHED_OTHER)
        return "OTHER";
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
        printf("Test FAILED. Error at pthread_attr_setschedparam(), errno=%d\n", errno);
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