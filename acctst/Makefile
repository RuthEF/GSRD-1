CC      = pgcc -c11 -Minfo
CCFLAGS =
ACCFLAGS= -acc -fast -ta=tesla:cc50

OBJ = o

# Other environments, compilers, options...
UNAME := $(shell uname -a)
ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC       = clang -std=c11 -W
ACCFLAGS =
endif
ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
EXE	= exe
endif

all: build run verify

build: acctst.c
	$(CC) $(CCFLAGS) $(ACCFLAGS) -o acctst $<

run: acctst
	./acctst 0

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf acctst *.o *.dwf *.pdb prof
