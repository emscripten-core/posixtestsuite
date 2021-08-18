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
 *	Asynchronous notification shall occur for AIO that are successfully
 *	cancelled.
 *
 * method:
 *
 *	we queue a lot of aio_write() with a valid sigevent to a file descriptor
 *	next we try to cancel all operations on this file descriptor
 *	we guess some have been finished, other are in progress,
 *	other are waiting
 *	we guess we can cancel all operations waiting
 *	then we analyze aio_error() in the event handler
 *	if aio_error() is ECANCELED, the test is passed
 *	otherwise, we don't know (perhaps we haven't cancel any operation ?)
 *	if number of sig event is not equal to number of aio_write()
 *	the test fails (in fact it hangs).
 *
 */

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include <aio.h>
#include <signal.h>

#include "posixtest.h"

#define TNAME "aio_cancel/3-1.c"

#define BUF_NB		128
#define BUF_SIZE	1024

static volatile int countdown = BUF_NB;
static volatile int canceled = 0;

void sigusr1_handler(int signum, siginfo_t *info, void *context)
{
	struct aiocb *a = info->si_value.sival_ptr;

	if (aio_error(a) == ECANCELED)
		canceled++;
		
	aio_return(a);	/* free entry */

	free((void*)a->aio_buf);
	free(a);

	countdown--;
}

int main()
{
	char tmpfname[256];
	int fd;
	struct aiocb *aiocb;
	struct sigaction action;
	int i;

#if _POSIX_ASYNCHRONOUS_IO != 200112L
	return PTS_UNSUPPORTED;
#endif

	snprintf(tmpfname, sizeof(tmpfname), "/tmp/pts_aio_cancel_3_1_%d", 
		  getpid());
	unlink(tmpfname);
	fd = open(tmpfname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		printf(TNAME " Error at open(): %s\n",
		       strerror(errno));
		return PTS_UNRESOLVED;
	}

	unlink(tmpfname);

	/* install signal handler */

	action.sa_sigaction = sigusr1_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO|SA_RESTART;
	if (sigaction(SIGRTMIN+1, &action, NULL))
	{
		printf(TNAME " Error at sigaction(): %s\n",
		       strerror(errno));
		return PTS_FAIL;
	}

	/* create AIO req */

	for (i = 0; i < BUF_NB; i++)
	{
		aiocb = malloc(sizeof(struct aiocb));
		if (aiocb == NULL)
		{
			printf(TNAME " Error at malloc(): %s\n",
		       		strerror(errno));
			return PTS_FAIL;
		}

		aiocb->aio_fildes = fd;
		aiocb->aio_buf = malloc(BUF_SIZE);
		if (aiocb->aio_buf == NULL)
		{
			printf(TNAME " Error at malloc(): %s\n",
		       		strerror(errno));
			return PTS_FAIL;
		}

		aiocb->aio_nbytes = BUF_SIZE;
		aiocb->aio_offset = 0;

		aiocb->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
		aiocb->aio_sigevent.sigev_signo = SIGRTMIN+1;
		aiocb->aio_sigevent.sigev_value.sival_ptr = aiocb;

		if (aio_write(aiocb) == -1)
		{
			printf(TNAME " loop %d: Error at aio_write(): %s\n",
			       i, strerror(errno));
			return PTS_FAIL;
		}
	}

	/* try to cancel all
	 * we hope to have enough time to cancel at least one
	 */

	if (aio_cancel(fd, NULL) == -1)
	{
		printf(TNAME " Error at aio_cancel(): %s\n",
		       strerror(errno));
		return PTS_FAIL;
	}

	close(fd);

	while(countdown);

	if (!canceled)
		return PTS_UNRESOLVED;

	printf ("Test PASSED\n");
	return PTS_PASS;
}
