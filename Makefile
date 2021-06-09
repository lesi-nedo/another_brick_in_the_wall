
DIR_SERV_MAIN=./server/src/
DIR_CLNT_MAIN=./clients/
SUBDIR= ${DIR_SERV_MAIN} ${DIR_CLNT_MAIN}

PHONY: clean all
.SUFFIXES: .c .h

all: 
	@-$(MAKE) --no-print-directory -C ${DIR_SERV_MAIN} all
	@-$(MAKE) --no-print-directory -C ${DIR_CLNT_MAIN} all

clean:
	@-${RM} ~core *.out *~ core
	@-${RM} -r logs
	@for dir in $(SUBDIR); do \
		$(MAKE) --no-print-directory -C $$dir clean; \
	done
