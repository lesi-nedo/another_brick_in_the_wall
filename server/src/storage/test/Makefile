LIBS=-lcheck -lm -lpthread -lrt -lsubunit
LIB_FLAG=-L
CC=gcc
FLAGS+= -Wall -pedantic -Werror
CFLAGS=-I
DEBUG=-g -DDEBUG
OPT=-O3
STD=-std=c11
MACR +=-D_POSIX_C_SOURCE=200112L
TEST=test
CHECK=check_stor
CUR_DIR:=$(dir $(realpath $(lastword $(MAKEFILE_LIST))))
CK_FORK=valgrind --leak-check=full
DIPEND=../new.c ../files_s.c ../../parse_api/thread_for_log.c
TOOBJ=$(patsubst %.c, %.o, $(notdir $(DIPEND)))
DIP_COV+=$(notdir $(DIPEND)) check_stor.c

GCOV=gcov
FLA=-k
COV=-fprofile-arcs -ftest-coverage

DIFF=./sort.sh
COV_NAM=test_store


.PHONY: all clean
.SUFFIXES: .c .o

all: $(TOOBJ) $(CHECK).o
	@printf "\033[\033[1;34mIf you see an error: Test timeout expired, just go to check_stor.c and \033[1;36mINCREMENT_THIS\033[1;34m then rerrun it.\033[0;37m\033[1;31mSome of the assertions might fail because of pthread_kill.\033[0;37m\n"
	@$(CC)  $(DEBUG) $^ $(LIBS) -o $(TEST).out
	@-${RM} *.o
	@printf "\033[0;37m"
	@-./$(TEST).out
	@-$(DIFF)
	@printf "\033[1;35m"
	@echo "Do you want run the test with valgrind?\033[0m" [y/n]
	@printf "\033[1;37m"
	@-read  line; if [ $$line = "y" ]; then printf "\033[1;37m"; $(CK_FORK) ./$(TEST).out; printf "\033[1;33mMemory still reachable is due to the framework, for more info: \033[1;32mhttps://libcheck.github.io/check/doc/check_html/check_4.html#Finding-Memory-Leaks\033[0;37m\n"; fi
	@$(MAKE) --no-print-directory clean

$(TOOBJ): $(DIPEND)
	@for dir in $^; do \
		$(CC) $(FLAGS) $(DEBUG) -c $$dir; \
	done

${CHECK}.o: $(CHECK).c
	@$(CC)  $(FLAGS) $(DEBUG)  -c $<

clean:
	@-${RM} *.o *.out *~ core $(COV_NAM) *.html *.txt *.gcda *.gcno *gcov
	@-${RM} -rf ../logs 