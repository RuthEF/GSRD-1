# GSRD makefile

# PGI C compiler is default
CC       = pgcc -c11 -Minfo=all
CCFLAGS  =
#-DLAPLACE_FUNCTION
# PGI Acceleration options
ACCFLAGS = -fast -acc
HAFLAGS  = $(ACCFLAGS) -ta=host $(OPT)
GAFLAGS  = $(ACCFLAGS) -ta=nvidia:cc50 $(OPT)
HGAFLAGS = $(ACCFLAGS) -ta=host,nvidia:cc50 $(OPT)
# -ta=tesla:managed -> malloc: call to cuMemAllocManaged returned error 3: Not initialized

OBJ = o
EXE = out
RUN =

# Other environments, compilers, options...
UNAME := $(shell uname -a)
ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC       = clang -std=c11 -W
ACCFLAGS =
endif
ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
OBJ	= obj
EXE	= exe
endif

# top level targets
all:     build run verify
host:    acchost run verify
gpu:     accgpu run verify
hostgpu: acchostgpu run verify

SRC_DIR=src
OBJ_DIR=obj

SL= gsrd.c proc.c data.c util.c
SRC:= $(SL:%.c=$(SRC_DIR)/%.c)
OBJ:= $(SL=:%.c=$(OBJ_DIR)/%.o)

# Default build - no acceleration
build: $(SRC)
	$(CC) $(CCFLAGS) -o gsrd.$(EXE) $(SRC)

acchost: $(SRC)
	$(CC) $(CCFLAGS) $(HAFLAGS) -o gsrd.$(EXE) $(SRC)

accgpu: $(SRC)
	$(CC) $(CCFLAGS) $(GAFLAGS) -o gsrd.$(EXE) $(SRC)

acchostgpu: $(SRC)
	$(CC) $(CCFLAGS) $(HGAFLAGS) -o gsrd.$(EXE) $(SRC)

run: gsrd.$(EXE)
	$(RUN) ./gsrd.$(EXE) "init/gsrd00000(1024,1024,2)F64.raw" -A:A -I=2000,500

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf *.$(EXE) *.$(OBJ) *.dwf *.pdb prof
