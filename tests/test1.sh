#!/bin/bash

set -m

MY_ID=./my_id.txt
DIR_F=../tests/tests
SOCK_NAME=$(cat ./tests/conf1/config.txt | grep SOCKET_NAME | cut -d '=' -f 2 | cut -d '"' -f 2)
DIR_F2=../tests/src
DIR_F3=../tests/libs
DIR_R4=../tests/files
CHILD=()


if [[ $1 -eq 1 ]]; then
    mv ./tests/conf1/config.txt ./server/ > /dev/null 2>&1
    cd ./server
    valgrind --leak-check=full ./main &
    SER=$!
    cd ../clients
fi
if [[ $1 -eq 0 ]]; then 
    cd ./clients
fi

#settin our id
echo 201 > $MY_ID
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -W $DIR_F2/csslujAcx.txt,$DIR_F2/dragqlEzq.txt,$DIR_F2/GoIEpzIEp.txt,$DIR_F2/iEyB.txt
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -W $DIR_F2/tgybBaBkr.txt,$DIR_F2/Idkv.txt,$DIR_F/umDmKqbeJ.txt

#writes files to the server
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -w $DIR_F n=40
#Sends all files with option -D 
sleep 1
#reading files from server without saving it
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -r $DIR_F2/csslujAcx.txt,$DIR_F2/dragqlEzq.txt,$DIR_F2/GoIEpzIEp.txt,$DIR_F2/iEyB.txt
#read at most 10 files to the folder libs
./main  ${2:-t} ${3:0} $4 -f $SOCK_NAME -d  $DIR_F3 -R n=10
#let's change the id and try to get the lock
echo 202 > my_id.txt
sleep 1
#will get stuck
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -l $DIR_F2/dragqlEzq.txt &
#change back the id to unlock
echo 201 > my_id.txt
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -u  $DIR_F2/dragqlEzq.txt
#deleting the file 
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -c  $DIR_F2/GoIEpzIEp.txt
#will fail
./main  ${2:-t} ${3:0} $4 -f $SOCK_NAME -r $DIR_F2/GoIEpzIEp.txt

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -l $DIR_F2/tgybBaBkr.txt &

./main  ${2:-t} ${3:0} $4 -f $SOCK_NAME -r $DIR_F2/tgybBaBkr.txt


./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -u  $DIR_F2/tgybBaBkr.txt

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -l $DIR_F/nrplEouJJ.txt &

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -u  $DIR_F/nrplEouJJ.txt

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -c  $DIR_F/nrplEouJJ.txt

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -l $DIR_F/nrplEouJJ.txt &

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -c  $DIR_F/nrplEouJJ.txt




sleep 2


if [[ $1 -eq 1 ]]; then
    kill -s SIGHUP ${SER} > /dev/null 2>&1
    
    cd ..
    mv ./server/config.txt ./tests/conf1/ > /dev/null 2>&1
fi
for i in "${CHILD[@]}"; do
    kill -9 ${i} > /dev/null 2>&1
    wait ${i}
done
echo -e "\033[1;32mDone\033[0m"