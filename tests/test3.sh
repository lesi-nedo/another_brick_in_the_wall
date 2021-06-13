#!/bin/bash


set -m


mv ./tests/conf3/config.txt ./server/

cd ./server
./main &
SER=$!
sleep 1
pids=()

set="abcdefghijklmonpqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
n=60000

function clients() {
    for i in {1..5}; do
        cd clients
        $(./main -f some_name -w ../tests/files > /dev/null 2>&1 &)
        $(./main -f some_name -R > /dev/null 2>&1)
        for fl in ../tests/files/*; do
            echo 201 > my_id.txt
            ./main -f some_name -w $fl > /dev/null 2>&1 &
            ./main -f some_name -r $fl > /dev/null 2>&1 &
            ./main -f some_name -u $fl > /dev/null 2>&1 &
            ./main -f some_name -c $fl > /dev/null 2>&1 &
        done
        for fl in ../tests/files/*; do
            ./main -f some_name -l $fl > /dev/null 2>&1 &
        done
        pids+=($!)
        c=$((201+i+2))
        cd ..
        if [[ $(expr $i % 2) -eq 0 ]]; then
            ./tests/test1.sh 0 -t 0 > /dev/null 2>&1 &
        else
            c=$((201+i))
            #changes client id's
            #some of calls will fail and it is ok
            # Even without -p the client will report an error but minimal.
            echo $c > ./clients/my_id.txt
            ./tests/test2.sh 0  >  /dev/null 2>&1 &
            pids+=($!)
        fi
    done

}



cd ..
clients &
pids+=($!)


sleep 35 && kill -s SIGINT "$SER" > /dev/null 2>&1



for i in "${pids[@]}"; do
    kill -9 ${i} > /dev/null 2>&1
    wait ${i}
done

mv ./server/config.txt ./tests/conf3/