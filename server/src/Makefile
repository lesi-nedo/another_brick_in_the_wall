CC=gcc
FLAGS=-Wall -Werror -pedantic
OPT=-O0
DEBUG=-g
STD=-std=c11 -D_POSIX_C_SOURCE=20112L
GNU=-D_GNU_SOURCE
LIB=-lpthread
DEB=-DDEBUG

DIR_LINK=./parse_api/
DIR_STOR=./storage/
DIR_THR=./threads/
DIR_SOCK=./sockets/

TARGET=main
LOG_TARGET=thread_for_log
PARSE_TARGET=parsing_conf
TARG_STOR=files_s
TARG_CACH=new
TARG_THR=bunch_of_threads
TARG_SOCK=api_sock.c

SUBDIR=parse_api/test storage/test

SHELL=/bin/bash
LIBS_TO_CHECK:=checkinstall
COMM:= $(shell whereis $(LIBS_TO_CHECK))
IS_THERE=$(COMM)

SOCKET_NAME=../some_name
MAIN=main
.PHONY: clean check all
.SUFFIXES: .c .h

all: ${TARGET}

${TARGET}: ${TARGET}.c  $(DIR_LINK)${LOG_TARGET}.c $(DIR_LINK)$(PARSE_TARGET).c\
${DIR_STOR}${TARG_STOR}.c ${DIR_STOR}${TARG_CACH}.c $(DIR_THR)$(TARG_THR).c $(DIR_SOCK)$(TARG_SOCK)
	@${CC} ${FLAGS} ${DEB} ${OPT} ${GNU} ${DEBUG} ${STD} $^ -o ../$@ ${LIB}

check:
ifeq ("$(COMM)", "$(LIBS_TO_CHECK):")
	@-printf "\033[1;31mCould not find unit testing framework check library, \033[1;33m visit: \033[1;35m https://libcheck.github.io/check/ \033[0;37m\n"
else
	@for dir in $(SUBDIR); do \
		$(MAKE) --no-print-directory -C $$dir; \
	done
endif

clean:
	@-${RM} ~core *.out *~ core $(TARGET) ${SOCKET_NAME} ${MAIN}
	@for dir in $(SUBDIR); do \
		$(MAKE) --no-print-directory -C $$dir clean; \
	done
