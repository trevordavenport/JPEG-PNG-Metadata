# Makefile for compiling your solution to Project 1.

# Project
EXECUTABLE      := ./analyze
SRC		:= $(sort $(wildcard *.c))
HDR		:= $(sort $(wildcard *.h))

# Compiler
WFLAGS		:= -Wall -Werror
FLAGS           := -g -O0 -fstack-protector-all -m64
LIBRARIES	:= -lz

# Targets
all: $(EXECUTABLE) test

$(EXECUTABLE): $(SRC) $(HDR)
	gcc -o $(EXECUTABLE) $(WFLAGS) $(FLAGS) $(SRC) $(LIBRARIES)

test: functionality-tests security-tests

security-tests:
	./run-sec-tests

functionality-tests:
	./run-fun-tests

clean:
	rm -f $(EXECUTABLE)
