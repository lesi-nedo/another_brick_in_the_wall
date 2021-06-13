CC=gcc
FLAGS=-Wall -Werror -pedantic
OPT=-O0
DEBUG=-g
STD=-std=c11 -D_POSIX_C_SOURCE=20112L
GNU=-D_GNU_SOURCE
LIB=-lpthread

DIR_SOCK=../server/src/sockets/

TARG_SOCK=api_sock
TARGET=main


.PHONY: all clean 
.SUFFIXES: .c .h

all: ${TARGET}

${TARGET}: ${TARGET}.c ${DIR_SOCK}${TARG_SOCK}.c
	@${CC} ${FLAGS} ${OPT} ${GNU} ${DEBUG} ${STD} $^ -o $@ ${LIB}

clean:
	@-${RM} ${TARGET}