CC=gcc
FLAGS=-Wall -pedantic
OPT=-O0
DEBUG=-g
STD=-std=c11 -D_POSIX_C_SOURCE=20112L
GNU=-D_GNU_SOURCE
LIB=-lpthread

DIR_LINK=./parse_api/
TARGET=main
LOG_TARGET=thread_for_log
PARSE_TARGET=parsing_conf

.PHONY: clean
.SUFFIXES: .c .h

all: ${TARGET}

${TARGET}: ${TARGET}.c  $(DIR_LINK)${LOG_TARGET}.c $(DIR_LINK)$(PARSE_TARGET).c
	@${CC} ${FLAGS} ${OPT} ${GNU} ${DEBUG} ${STD} $^ -o $@ ${LIB}

clean:
	${RM} ~core *.out *~ core $(TARGET)
