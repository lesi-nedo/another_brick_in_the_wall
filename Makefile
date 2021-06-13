
DIR_SERV_MAIN=./server/src/
DIR_CLNT_MAIN=./clients/
SUBDIR= ${DIR_SERV_MAIN} ${DIR_CLNT_MAIN}

.PHONY: all  clean ${DIR_SERV_MAIN} ${DIR_CLNT_MAIN} test1 test2 test3 check_store
.SUFFIXES: .c .h

all: ${DIR_SERV_MAIN} ${DIR_CLNT_MAIN}

	@-echo  "\033[1;36mIt will create executable for the client in the clients folder, for the server in the server folder.\033[0m"

$(DIR_SERV_MAIN):
	@$(MAKE) --no-print-directory -C $@ all
$(DIR_CLNT_MAIN):
	@$(MAKE) --no-print-directory -C $@ all

clean:
	@-${RM} ~core *.out *~ core
	@-${RM} -r logs
	@${RM} ./tests/victims/*.txt
	@${RM} ./tests/libs/*.txt

	@for dir in $(SUBDIR); do \
		$(MAKE) --no-print-directory -C $$dir clean; \
	done

test1: $(DIR_SERV_MAIN) ${DIR_CLNT_MAIN}
	@-./tests/test1.sh 1 -t 200 -p
	@-$(mv ./server/config.txt ./tests/conf1/)

test2: $(DIR_SERV_MAIN) ${DIR_CLNT_MAIN}
	@-./tests/test2.sh 1 -t 200 -p
	@-$(mv ./server/config.txt ./tests/conf2/)

test3: $(DIR_SERV_MAIN) ${DIR_CLNT_MAIN}
	@-./tests/test3.sh
	@-$(mv ./server/config.txt ./tests/conf2/)

#check storege wti
check_store: $(DIR_SERV_MAIN)/storage/test
	@$(MAKE) --no-print-directory -C $<