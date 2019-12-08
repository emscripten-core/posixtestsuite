/*
 * Copyright (c) 2004, Bull SA. All rights reserved.
 * Created by:  Laurent.Vivier@bull.net
 * This file is licensed under the GPL license.  For the full content
 * of this license, see the COPYING file at the top level of this 
 * source tree.
 */

/*
 * assertion:
 *
 *	The aio_suspend() function shall suspend the calling thread until at
 *	least one of the asynchronous I/O operations referenced by the list
 *	argument has completed, until a signal interrupts the function, or,
 *	if timeout is not NULL, until the time interval specified by timeout
 *	has passed.
 *
 * method: Testing for a NULL timeout
 *
 *	- write to a file
 *	- submit a list of read requests
 *	- check that the selected request has not completed
 *	- suspend on selected request
 *	- check that the selected request has completed using aio_error and
 *	  aio_return
 *
 */

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

#include <signal.h>
#include <fcntl.h>
#include "posixtest.h"

#define TNAME "aio_suspend/8-1.c"

#define NUM_AIOCBS	10
#define BUF_SIZE	1024*1024
#define WAIT_FOR_AIOCB	6

int received_selected	= 0;
int received_all	= 0;

void
sigrt1_handler(int signum, siginfo_t *info, void *context)
{
	if (info->si_value.sival_int == WAIT_FOR_AIOCB)
		received_selected = 1;
}

void
sigrt2_handler(int signum, siginfo_t *info, void *context)
{
	received_all = 1;
}

int
main ()
{
	char tmpfname[256];
	int fd;

	struct aiocb **aiocbs;
	struct aiocb *plist[2];
	char *bufs;
	struct sigaction action;
	struct sigevent event;
	int errors = 0;
	int ret;
	int err;
	int i;

#if _POSIX_ASYNCHRONOUS_IO != 200112L
	exit(PTS_UNSUPPORTED);
#endif

	snprintf(tmpfname, sizeof(tmpfname), "/tmp/pts_aio_suspend_8_1_%d", 
		  getpid());
	unlink(tmpfname);

	fd = open(tmpfname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);

	if (fd == -1) {
		printf(TNAME " Error at open(): %s\n",
		       strerror(errno));
		exit(PTS_UNRESOLVED);
	}

	unlink(tmpfname);

	bufs = (char *) malloc (NUM_AIOCBS*BUF_SIZE);

	if (bufs == NULL) {
		printf (TNAME " Error at malloc(): %s\n", strerror (errno));
		close (fd);
		exit(PTS_UNRESOLVED);
	}

	if (write (fd, bufs, NUM_AIOCBS*BUF_SIZE) != (NUM_AIOCBS*BUF_SIZE)) {
		printf(TNAME " Error at write(): %s\n", strerror(errno));
		free (bufs);
		close (fd);
		exit(PTS_UNRESOLVED);
	}



	aiocbs = (struct aiocb**)malloc(sizeof(struct aiocb *) * NUM_AIOCBS);

	/* Queue up a bunch of aio reads */
	for (i = 0; i < NUM_AIOCBS; i++) {

		aiocbs[i] = (struct aiocb*)malloc(sizeof(struct aiocb));
		memset(aiocbs[i], 0, sizeof(struct aiocb));

		aiocbs[i]->aio_fildes = fd;
		aiocbs[i]->aio_offset = i * BUF_SIZE;
		aiocbs[i]->aio_buf = &bufs[i*BUF_SIZE];
		aiocbs[i]->aio_nbytes = BUF_SIZE;
		aiocbs[i]->aio_lio_opcode = LIO_READ;

		/* Use SIRTMIN+1 for individual completions */
		aiocbs[i]->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
		aiocbs[i]->aio_sigevent.sigev_signo = SIGRTMIN+1;
		aiocbs[i]->aio_sigevent.sigev_value.sival_int = i;
	}

	/* Use SIGRTMIN+2 for list completion */
	event.sigev_notify = SIGEV_SIGNAL;
	event.sigev_signo = SIGRTMIN+2;
	event.sigev_value.sival_ptr = NULL;

	/* Setup handler for individual operation completion */
	action.sa_sigaction = sigrt1_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO|SA_RESTART;
	sigaction(SIGRTMIN+1, &action, NULL);

	/* Setup handler for list completion */
	action.sa_sigaction = sigrt2_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO|SA_RESTART;
	sigaction(SIGRTMIN+2, &action, NULL);

	/* Setup suspend list */
	plist[0] = NULL;
	plist[1] = aiocbs[WAIT_FOR_AIOCB];

	/* Submit request list */
	ret = lio_listio(LIO_NOWAIT, aiocbs, NUM_AIOCBS, &event);

	if (ret) {
		printf(TNAME " Error at lio_listio() %d: %s\n", errno, strerror(errno));
		for (i=0; i<NUM_AIOCBS; i++)
			free (aiocbs[i]);
		free (bufs);
		free (aiocbs);
		close (fd);
		exit (PTS_UNRESOLVED);
	}

	/* Check selected request has not completed yet */
	if (received_selected) {
		printf (TNAME " Error : AIOCB %d already completed before suspend\n",
			WAIT_FOR_AIOCB);
		for (i=0; i<NUM_AIOCBS; i++)
			free (aiocbs[i]);
		free (bufs);
		free (aiocbs);
		close (fd);
		exit (PTS_FAIL);
	}

	/* Suspend on selected request */
	ret = aio_suspend((const struct aiocb **)plist, 2, NULL);

	if (ret) {
		printf (TNAME " Error at aio_suspend() %d: %s\n", errno, strerror (errno));
		for (i=0; i<NUM_AIOCBS; i++)
			free (aiocbs[i]);
		free (bufs);
		free (aiocbs);
		close (fd);
		exit (PTS_FAIL);
	}

	/* Check selected request has completed */
	err = aio_error (aiocbs[WAIT_FOR_AIOCB]);
	ret = aio_return (aiocbs[WAIT_FOR_AIOCB]);
	
	if ((err != 0) && (ret !=  BUF_SIZE)) {
		printf (TNAME " Error : AIOCB %d should have completed after suspend\n",
			WAIT_FOR_AIOCB);
		for (i=0; i<NUM_AIOCBS; i++)
			free (aiocbs[i]);
		free (bufs);
		free (aiocbs);
		close (fd);
		exit (PTS_FAIL);
	}


	/* Wait for list processing completion */
	while (!received_all)
		sleep (1);

	/* Check return code and free things */
	for (i = 0; i < NUM_AIOCBS; i++) {
		if (i == WAIT_FOR_AIOCB)
			continue;

	  	err = aio_error(aiocbs[i]);
		ret = aio_return(aiocbs[i]);

		if ((err != 0) && (ret != BUF_SIZE)) {
			printf(TNAME " req %d: error = %d - return = %d\n", i, err, ret);
			errors++;
		}

		free (aiocbs[i]);
	}

	free (bufs);
	free (aiocbs);

	close(fd);

	if (errors != 0)
		exit (PTS_FAIL);

	printf (TNAME " PASSED\n");

	return PTS_PASS;
}
