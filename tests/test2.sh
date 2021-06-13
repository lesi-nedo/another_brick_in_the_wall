#!/bin/bash

set -m

DIR_F=../tests/tests
DIR_F2=../tests/src
DIR_F3=../tests/libs
SOCK_NAME=$(cat ./tests/conf2/config.txt | grep SOCKET_NAME | cut -d '=' -f 2 | cut -d '"' -f 2)
pids=()


if [[ $1 -eq 1 ]]; then
    mv ./tests/conf2/config.txt ./server/ 
    cd ./server
    ./main &
    SER=$!
    sleep 1
    cd ../clients
fi


if [[ $1 -eq 0 ]]; then 
    cd ./clients
fi

./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -w $DIR_F n=8
#Server will not send back data if not modified so we make sure at least some files are.
#each writes appends data.
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -W $DIR_F2/EHkuuvJyI.txt,$DIR_F2/EjpBEhiIK.txt,$DIR_F2/EhBkrFugI.txt,$DIR_F2/sCcd.txt,$DIR_F2/Rpdv.txt,$DIR_F2/ucplouDom.txt,$DIR_F2/yohrIaKlz.txt 
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -W $DIR_F2/EHkuuvJyI.txt,$DIR_F2/EjpBEhiIK.txt,$DIR_F2/EhBkrFugI.txt,$DIR_F2/sCcd.txt,$DIR_F2/Rpdv.txt,$DIR_F2/ucplouDom.txt,$DIR_F2/yohrIaKlz.txt 
./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -W $DIR_F2/EHkuuvJyI.txt,$DIR_F2/EjpBEhiIK.txt,$DIR_F2/EhBkrFugI.txt,$DIR_F2/sCcd.txt,$DIR_F2/Rpdv.txt,$DIR_F2/ucplouDom.txt,$DIR_F2/yohrIaKlz.txt 
#know we should get someting back and we save it in the folder victims
# ./main ${2:-t} ${3:0} $4 -f $SOCK_NAME -w $DIR_F3 n=10 -D ../tests/victims
# Know I'll bombard my server
#Some of this calls might fail because probably a client will try to create/lock already exited files 
for i in {1..20}; do
    if [[ $(expr $i % 3) -eq 0 ]]; then
        ./main $4 -f $SOCK_NAME -D ../tests/victims -w $DIR_F  &
        pid+=($!)
    elif [[ $(expr $i % 3) -eq 1 ]]; then
        ./main $4 -f $SOCK_NAME -D ../tests/victims -w $DIR_F2 &
        pid+=($!)
    elif [[ $(expr $i % 3) -eq  2 ]]; then 
        ./main $4 -f $SOCK_NAME -D ../tests/victims -w $DIR_F3  &
        pid+=($!)
    fi
done

cd ..
sleep 3
if [[ $1 -eq 1 ]]; then
    kill -s SIGHUP "$SER" > /dev/null 2>&1
    mv ./server/config.txt ./tests/conf2/ > /dev/null 2>&1
fi
for i in "${pids[@]}"; do
    echo "${pids[@]}"
    kill -9 ${i}
    wait ${i}
done
echo -e "\033[1;32mDone\033[0m"
