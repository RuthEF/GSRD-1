# GSRD makefile

# PGI C compiler is default
CC       = pgcc
CCFLAGS  = -c11 -Minfo=all

# PGI Acceleration options
ACCFLAGS = -fast -acc=verystrict
#ACCFLAGS = -O4 -Mautoinline -acc=verystrict
#OPT = -ta=tesla:managed #ERR: malloc: call to cuMemAllocManaged returned error 3: Not initialized
MAFLAGS  = $(ACCFLAGS) -ta=multicore $(OPT)
GAFLAGS  = $(ACCFLAGS) -ta=nvidia:cc50 $(OPT)
MGAFLAGS = $(ACCFLAGS) -ta=multicore,nvidia:cc50 $(OPT)
RUNFLAGS = -A:A -I=2000,500

TARGET = gsrd
DATAFILE = "init/gsrd00000(1024,1024,2)F64.raw"
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

# Fall back to standard LINUX tools if necessary
ifeq ($(findstring "not found", $(CCOUT)), "not found")
CC       = gcc
CCFLAGS  = -std=c11 -W
ACCFLAGS =
RUNFLAGS = -A:N -I=2000,500
endif

ifeq ($(findstring RPi, $(UNAME)), RPi)
CC       = gcc
CCFLAGS  = -std=c11 -W
DATAFILE = "init/gsrd00000(128,128,2)F64.raw"
RUNFLAGS = -A:N -I=200,50
endif

# Other environments, compilers, options...
ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC = clang
CCFLAGS  = -std=c11 -W
DATAFILE = "init/gsrd00000(512,512,2)F64.raw"
RUNFLAGS = -A:N -I=2000,500
endif

ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
OBJ = obj
TARGET = $(TARGET).exe
endif


# top level targets
all:     build run verify
host:    acchost run verify
gpu:     accgpu run verify
hostgpu: acchostgpu run verify

SRC_DIR=src
OBJ_DIR=obj

#SL = $(shell ls $(SRC_DIR))
SL= gsrd.c proc.c data.c util.c
SRC:= $(SL:%.c=$(SRC_DIR)/%.c)
OBJ:= $(SL=:%.c=$(OBJ_DIR)/%.o)

# Default build - no acceleration
build: $(SRC)
	$(CC) $(CCFLAGS) -o $(TARGET) $(SRC)

acchost: $(SRC)
	$(CC) $(CCFLAGS) $(MAFLAGS) -o $(TARGET) $(SRC)

accgpu: $(SRC)
	$(CC) $(CCFLAGS) $(GAFLAGS) -o $(TARGET) $(SRC)

acchostgpu: $(SRC)
	$(CC) $(CCFLAGS) $(MGAFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) $(DATAFILE) $(RUNFLAGS)

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
