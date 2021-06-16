#!/bin/bash


set -m


mv ./tests/conf3/config.txt ./server/

cd ./server
./main &
SER=$!
sleep 1
pids=()


function clients() {
    for i in {1..5}; do
        cd clients
        $(./main -f some_name -w ../tests/files > /dev/null 2>&1 &)
        $(./main -f some_name -R > /dev/null 2>&1)
         for fl in ../tests/files/*; do
            echo 201 > my_id.txt
            if [[ $(expr $i % 2) -eq 0 ]]; then
                ./main -f some_name -w $fl -r $fl -R -u $fl -l $fl -c $fl > /dev/null 2>&1 &
                pids+=($!)
                ./main -f some_name -w $fl -r $fl -R -u $fl -c $fl -u $fl > /dev/null 2>&1 &
                pids+=($!)
            elif [[ $(expr $i % 3) -eq 0 ]]; then
                ./main -f some_name -w $fl -r $fl -R -u $fl -l $fl -c $fl  > /dev/null 2>&1 &
                pids+=($!)
                sleep 0.1
            else
                ./main -f some_name -w $fl -r $fl -R -u $fl -l $fl -c $fl > /dev/null 2>&1 &
                pids+=($!)
                sleep 0.1
            fi
            
        done
        # for fl in ../tests/files/*; do
        #     ./main -f some_name -l $fl > /dev/null 2>&1 &
        # done
        cd ..
        if [[ $(expr $i % 2) -eq 0 ]]; then
            ./tests/test1.sh 0 -t 0 -R > /dev/null 2>&1 &
            pids+=($!)
        fi
    done

}



cd ..
clients &
pids+=($!)


sleep 25 && kill -s SIGINT "$SER" > /dev/null 2>&1



for i in "${pids[@]}"; do
    kill -9 ${i} > /dev/null 2>&1
    wait ${i}
done

mv ./server/config.txt ./tests/conf3/