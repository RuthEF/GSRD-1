CC      = pgcc
CCFLAGS = -c11 -Minfo=all
ACCFLAGS= -acc -fast -ta=tesla:cc50
TARGET= acctst
OBJEXT = o

# Other environments, compilers, options...
UNAME := $(shell uname -a)

ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
TARGET = $(TARGET).exe
OBJEXT = obj
endif

ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC      = clang
CCFLAGS = -std=c11 -W
ACCFLAGS=
endif

# Check compiler & fall back to standard LINUX tools if necessary
CCOUT := $(shell $(CC) 2>&1)

ifeq ($(findstring "not found", $(CCOUT)), "not found")
# Catch-all disnae work...
CC       = gcc
CCFLAGS  = -std=c11 -W
ACCFLAGS =
endif

ifeq ($(findstring RPi, $(UNAME)), RPi)
CC       = gcc
CCFLAGS  = -std=c11 -W
ACCFLAGS =
endif

all: build run verify

build: acctst.c
	$(CC) $(CCFLAGS) $(ACCFLAGS) -o $(TARGET) $<

run: $(TARGET)
	./$(TARGET) 0

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) *.$(OBJEXT) *.dwf *.pdb prof
