# Copyright (c) 2004, Intel Corporation. All rights reserved.
# Created by:  HongJunxin
# This file is licensed under the GPL license.  For the full content
# of this license, see the COPYING file at the top level of this
# source tree.

INCLUDE = -Wall -I../../../include

CFLAGS =

LDFLAGS = -lpthread

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.test)

all: do_compile

do_compile: $(OBJ)

%.test: %.c
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@ $(LDFLAGS)
	
clean:
	@rm -f $(OBJ)

	@if test -z "$(CC)"; then \
		CC=gcc; \
	fi	
