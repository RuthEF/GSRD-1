
################################################################################
#
# Copyright (c) 2017, NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.
#
################################################################################

CC       = pgcc -fast
CCFLAGS  = -DLAPLACE_FUNCTION
ACCFLAGS = -Minfo=all -acc $(OPT)
# -ta=tesla:managed -> malloc: call to cuMemAllocManaged returned error 3: Not initialized

OBJ	= o
EXE	= out
RUN     =

UNAME := $(shell uname -a)
ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC       = clang -std=c11 -W
ACCFLAGS =
endif
ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
OBJ	= obj
EXE	= exe
endif

all: build run verify

SRC_DIR=src

SL= gsrd.c proc.c data.c util.c
SRC:= $(SL:%.c=$(SRC_DIR)/%.c)


build: $(SRC)
	$(CC) $(CCFLAGS) $(ACCFLAGS) -o gsrd.$(EXE) $(SRC)

run: gsrd.$(EXE)
	$(RUN) ./gsrd.$(EXE) "init/gsrd00000(512,512,2)F64.raw"

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf *.$(EXE) *.$(OBJ) *.dwf *.pdb prof
