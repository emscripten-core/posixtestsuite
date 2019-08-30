/*
* author: HongJunxin
* 
* 测试项：优先级反转    
*
* 测试步骤：
* 1. 主线程初始化互斥量 mtx，使用优先级继承标志 PTHREAD_PRIO_INHERIT
* 2. 主线程创建线程B，优先级1（最低）, 限制在 cpu 1 运行，使用调度策略 SCHED_FIFO，
*    线程属性 PTHREAD_EXPLICIT_SCHED
* 3. 主线程创建线程A，优先级7（最高）, 限制在 cpu 1 运行，使用调度策略 SCHED_FIFO，
*    线程属性 PTHREAD_EXPLICIT_SCHED
* 4. 主线程创建线程C，优先级3（居中）, 限制在 cpu 1 运行，使用调度策略 SCHED_FIFO，
*    线程属性 PTHREAD_EXPLICIT_SCHED
*
* 5. 线程B获得互斥量mtx，然后进入while循环。
* 6. 线程A请求获取互斥量mtx，进入阻塞状态。
* 7. 线程C处于挂起状态，等待比它优先级高的线程运行结束。（如果线程B的优先级没有被提升，
*    则线程C不需要等待高优先级但处于阻塞的线程A。因为线程B的优先级被提升了并且处于运行
*    状态，所以此时线程等待的是线程B，但线程B释放了互斥量mtx，此时线程A运行，线程C又得
*    等待线程A运行结束。）
* 8. 线程B退出while循环，释放互斥量mtx。
* 9. 线程A得到互斥量mtx，运行结束
* 10. 线程C运行结束
* 11. 线程B运行结束
*
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

static pthread_mutex_t mtx;
static int is_thread_c_pause;
static int a_thread_status;
static int c_thread_status;

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
	
	set_cpu_affinity(1);
	
	printf("[Thread B] acquires mutex\n");
	s = pthread_mutex_lock(&mtx); 
	if (s != 0) {
		printf("[Thread B] failed to lock mutex, error caode=%d\n", s);
		exit(PTS_UNRESOLVED);
	}
	
	while (is_thread_c_pause);
	
	/* 线程A（优先级最高）因等待mutex而处于阻塞状态，如果线程B（优先级最低）的优先级
	   没有被提升，则线程C（优先级居中）早就已经运行完毕。但因为线程B发生了优先级
	   提升，而已一直处于运行状态，所以在这个点线程C应该是处于挂起状态，因为这三者使
	   用的调用策略是SCHED_FIFO */
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

	c_thread_status = RUNNING;
	printf("[Thread C] running...\n");

	set_cpu_affinity(1);

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
	
	/* PTHREAD_EXPLICIT_SCHED 需要 root 权限。默认是 PTHREAD_INHERIT_SCHED，
       这个会导致新建的线程继承主线程的属性，从而导致自己对新线程设置的优先级
	   和调度策略无效。*/
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

	/* 如果将 PTHREAD_PRIO_INHERIT 改为 PTHREAD_PRIO_NONE，该测试将不通过。 */
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
	pthread_t a_thread, b_thread, c_thread;
	
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