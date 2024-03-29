/*
* Copyright (c) 2005, Bull S.A..  All rights reserved.
* Created by: Sebastien Decugis

* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it would be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write the Free Software Foundation, Inc., 59
* Temple Place - Suite 330, Boston MA 02111-1307, USA.


* This sample test aims to check the following assertion:
*
* The function does not return EINTR

* The steps are:
* -> kill a thread which calls pthread_cancel
* -> check that EINTR is never returned

*/


/* We are testing conformance to IEEE Std 1003.1, 2003 Edition */
#define _POSIX_C_SOURCE 200112L

/********************************************************************************************/
/****************************** standard includes *****************************************/
/********************************************************************************************/
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <semaphore.h>
#include <errno.h>
#include <signal.h>

/********************************************************************************************/
/******************************   Test framework   *****************************************/
/********************************************************************************************/
#include "testfrmw.h"
#include "testfrmw.c" 
/* This header is responsible for defining the following macros:
 * UNRESOLVED(ret, descr);  
 *    where descr is a description of the error and ret is an int (error code for example)
 * FAILED(descr);
 *    where descr is a short text saying why the test has failed.
 * PASSED();
 *    No parameter.
 * 
 * Both three macros shall terminate the calling process.
 * The testcase shall not terminate in any other maneer.
 * 
 * The other file defines the functions
 * void output_init()
 * void output(char * string, ...)
 * 
 * Those may be used to output information.
 */

/********************************************************************************************/
/********************************** Configuration ******************************************/
/********************************************************************************************/
#ifndef VERBOSE
#define VERBOSE 1
#endif

#ifndef __EMSCRIPTEN__
// See https://github.com/emscripten-core/emscripten/issues/14892
#define WITH_SYNCHRO
#endif


/********************************************************************************************/
/***********************************    Test cases  *****************************************/
/********************************************************************************************/

char do_it = 1;
unsigned long count_ope = 0;
#ifdef WITH_SYNCHRO
sem_t semsig1;
sem_t semsig2;
unsigned long count_sig = 0;
#endif

sigset_t usersigs;

typedef struct
{
	int sig;
#ifdef WITH_SYNCHRO
	sem_t *sem;
#endif
}

thestruct;

/* the following function keeps on sending the signal to the process */
void * sendsig ( void * arg )
{
	thestruct * thearg = ( thestruct * ) arg;
	int ret;
	pid_t process;

	process = getpid();

	/* We block the signals SIGUSR1 and SIGUSR2 for this THREAD */
	ret = pthread_sigmask( SIG_BLOCK, &usersigs, NULL );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Unable to block SIGUSR1 and SIGUSR2 in signal thread" );
	}

	while ( do_it )
	{
#ifdef WITH_SYNCHRO

		if ( ( ret = sem_wait( thearg->sem ) ) )
		{
			UNRESOLVED( errno, "Sem_wait in sendsig" );
		}

		count_sig++;
#endif

		ret = kill( process, thearg->sig );

		if ( ret != 0 )
		{
			UNRESOLVED( errno, "Kill in sendsig" );
		}

	}

	return NULL;
}

/* Next are the signal handlers. */
/* This one is registered for signal SIGUSR1 */
void sighdl1( int sig )
{
#ifdef WITH_SYNCHRO

	if ( sem_post( &semsig1 ) )
	{
		UNRESOLVED( errno, "Sem_post in signal handler 1" );
	}

#endif
}

/* This one is registered for signal SIGUSR2 */
void sighdl2( int sig )
{
#ifdef WITH_SYNCHRO

	if ( sem_post( &semsig2 ) )
	{
		UNRESOLVED( errno, "Sem_post in signal handler 2" );
	}

#endif
}

/* Init function */
void * testth( void * arg )
{

	while ( do_it )
		sched_yield();

	return NULL;
}


/* Test function -- calls pthread_kill() and checks that EINTR is never returned. */
void * test( void * arg )
{
	int ret = 0;
	pthread_t test_ch;

	ret = pthread_create( &test_ch, NULL, testth, NULL );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Failed to create a thread" );
	}

	/* We don't block the signals SIGUSR1 and SIGUSR2 for this THREAD */
	ret = pthread_sigmask( SIG_UNBLOCK, &usersigs, NULL );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Unable to unblock SIGUSR1 and SIGUSR2 in worker thread" );
	}


	while ( do_it )
	{
		count_ope++;

		ret = pthread_cancel( test_ch );

		if ( ret == EINTR )
		{
			FAILED( "pthread_self returned EINTR" );
		}

		if ( ret != 0 )
		{
			UNRESOLVED( ret, "pthread_self returned an unexpected error" );
		}

	}


	ret = pthread_join( test_ch, NULL );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Failed to join the canceled thread" );
	}

	return NULL;
}

/* Main function */
int main ( int argc, char * argv[] )
{
	int ret;
	pthread_t th_work, th_sig1, th_sig2;
	thestruct arg1, arg2;

	struct sigaction sa;

	/* Initialize output routine */
	output_init();

	/* We need to register the signal handlers for the PROCESS */
	sigemptyset ( &sa.sa_mask );
	sa.sa_flags = 0;
	sa.sa_handler = sighdl1;

	if ( ( ret = sigaction ( SIGUSR1, &sa, NULL ) ) )
	{
		UNRESOLVED( ret, "Unable to register signal handler1" );
	}

	sa.sa_handler = sighdl2;

	if ( ( ret = sigaction ( SIGUSR2, &sa, NULL ) ) )
	{
		UNRESOLVED( ret, "Unable to register signal handler2" );
	}

	/* We prepare a signal set which includes SIGUSR1 and SIGUSR2 */
	sigemptyset( &usersigs );

	ret = sigaddset( &usersigs, SIGUSR1 );

	ret |= sigaddset( &usersigs, SIGUSR2 );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Unable to add SIGUSR1 or 2 to a signal set" );
	}

	/* We now block the signals SIGUSR1 and SIGUSR2 for this THREAD */
	ret = pthread_sigmask( SIG_BLOCK, &usersigs, NULL );

	if ( ret != 0 )
	{
		UNRESOLVED( ret, "Unable to block SIGUSR1 and SIGUSR2 in main thread" );
	}

#ifdef WITH_SYNCHRO
	if ( sem_init( &semsig1, 0, 1 ) )
	{
		UNRESOLVED( errno, "Semsig1  init" );
	}

	if ( sem_init( &semsig2, 0, 1 ) )
	{
		UNRESOLVED( errno, "Semsig2  init" );
	}

#endif

	if ( ( ret = pthread_create( &th_work, NULL, test, NULL ) ) )
	{
		UNRESOLVED( ret, "Worker thread creation failed" );
	}

	arg1.sig = SIGUSR1;
	arg2.sig = SIGUSR2;
#ifdef WITH_SYNCHRO
	arg1.sem = &semsig1;
	arg2.sem = &semsig2;
#endif



	if ( ( ret = pthread_create( &th_sig1, NULL, sendsig, ( void * ) & arg1 ) ) )
	{
		UNRESOLVED( ret, "Signal 1 sender thread creation failed" );
	}

	if ( ( ret = pthread_create( &th_sig2, NULL, sendsig, ( void * ) & arg2 ) ) )
	{
		UNRESOLVED( ret, "Signal 2 sender thread creation failed" );
	}



	/* Let's wait for a while now */
	sleep( 1 );


	/* Now stop the threads and join them */
	do
	{
		do_it = 0;
	}
	while ( do_it );


	if ( ( ret = pthread_join( th_sig1, NULL ) ) )
	{
		UNRESOLVED( ret, "Signal 1 sender thread join failed" );
	}

	if ( ( ret = pthread_join( th_sig2, NULL ) ) )
	{
		UNRESOLVED( ret, "Signal 2 sender thread join failed" );
	}


	if ( ( ret = pthread_join( th_work, NULL ) ) )
	{
		UNRESOLVED( ret, "Worker thread join failed" );
	}


#if VERBOSE > 0
	output( "Test executed successfully.\n" );

	output( "  %d operations.\n", count_ope );

#ifdef WITH_SYNCHRO
	output( "  %d signals were sent meanwhile.\n", count_sig );

#endif
#endif
	printf("Test PASS\n");
	PASSED;
}

