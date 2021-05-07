SHELL=/bin/bash
LIBS_TO_CHECK:=checkinstall
LIBS=-lcheck -lm -lpthread -lrt -lsubunit
LIB_FLAG=-L
CC=gcc
FLAGS+= -Wall -pedantic
CFLAGS=-I
DEBUG=-g
OPT=-O3
STD=-std=c11
MACR +=-D_POSIX_C_SOURCE=200809L
COMM:= $(shell whereis $(LIBS_TO_CHECK))
IS_THERE=$(COMM)
TEST=test
PARSE=parse
CHECK=check_parse
CUR_DIR:=$(dir $(realpath $(lastword $(MAKEFILE_LIST))))
CK_FORK=valgrind --leak-check=full




.PHONY: all clean
.SUFFIXES: .c .o

all: $(CHECK).o
ifeq ("$(COMM)", "$(LIBS_TO_CHECK):")
	@-printf "\033[1;31mCould not find unit testing framework check library, \033[1;33m visit: \033[1;35m https://libcheck.github.io/check/ \033[0;37m\n"
else
	@$(CC) $(PARSE).o $(CHECK).o $(LIBS) -o $(TEST).out
	@printf "\033[1;37m"
	@-./$(TEST).out
	@printf "\033[1;35m"
	@echo Do you want run the test with valgrind? [y/n]
	@printf "\033[1;37m"
	@-read  line; if [ $$line = "y" ]; then $(CK_FORK) ./$(TEST).out; printf "\033[1;33mMemory still reachable is a particular case of the framework, for more info: \033[1;32mhttps://libcheck.github.io/check/doc/check_html/check_4.html#Finding-Memory-Leaks\033[0;37m\n"; fi
	@$(MAKE) --no-print-directory clean
endif

${CHECK}.o: ../parsing_conf.c
	@$(CC) $(FLAGS) $(STD) $(MACR)  $^  -c -o $(PARSE).o
	@$(CC) $(FLAGS) $(STD)  $(CHECK).c  -c -o $(CHECK).o

clean:
	@${RM} *.o *.out *~ core
