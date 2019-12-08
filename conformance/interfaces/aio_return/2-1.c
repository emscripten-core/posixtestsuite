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
 *	aio_return() may be called exactly once to retrieve the return status.
 *
 * method:
 *
 *	- open a file
 *	- fill in an aiocb for writing
 *	- call aio_write using this aiocb
 *	- call aio_return to get the aiocb status (number of bytes written)
 *	- call aio_return again, return status should be -1
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

#define TNAME "aio_return/2-1.c"

int main()
{
	char tmpfname[256];
#define BUF_SIZE 111
	char buf[BUF_SIZE];
	int fd;
	struct aiocb aiocb;
	int retval;

#if _POSIX_ASYNCHRONOUS_IO != 200112L
	exit(PTS_UNSUPPORTED);
#endif

	snprintf(tmpfname, sizeof(tmpfname), "/tmp/pts_aio_return_2_1_%d", 
		  getpid());
	unlink(tmpfname);
	fd = open(tmpfname, O_CREAT | O_RDWR | O_EXCL,
		  S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		printf(TNAME " Error at open(): %s\n",
		       strerror(errno));
		exit(PTS_UNRESOLVED);
	}

	unlink(tmpfname);

	memset(buf, 0xaa, BUF_SIZE);
	memset(&aiocb, 0, sizeof(struct aiocb));
	aiocb.aio_fildes = fd;
	aiocb.aio_buf = buf;
	aiocb.aio_nbytes = BUF_SIZE;

	if (aio_write(&aiocb) == -1)
	{
		printf(TNAME " Error at aio_write(): %s\n",
		       strerror(errno));
		exit(PTS_FAIL);
	}

	close(fd);

	do {
		retval = aio_error( &aiocb);
		if (retval == -1)
		{
			printf(TNAME " Error at aio_error(): %s\n",
				strerror(errno));
			exit(PTS_FAIL);
		}
	} while (retval == EINPROGRESS);

	retval = aio_return(&aiocb);
	if (retval != BUF_SIZE)
	{
		printf(TNAME " Error at aio_return(): %s\n",
		       strerror(errno));
		exit(PTS_FAIL);
	}

	retval = aio_return(&aiocb);

	if (retval != -1)
	{
		printf(TNAME " Second call to aio_return() should return -1 : %d\n",
		       retval);
		exit(PTS_FAIL);
	}

	printf ("Test PASSED\n");
	return PTS_PASS;
}
