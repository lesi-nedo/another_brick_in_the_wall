#!/bin/bash
max=0
max_f=0
alg=0
con=0

LOG_FL=(./logs/*.log)

if [[ ! -d ./logs ]]; then
    echo -e "\033[1;31mRun First Makefile.\033[0m"
fi
if [[ ! -f $LOG_FL ]]; then
    echo -e "\033[1;31mCould not find log file, pass it as argument.\033[0m"
    exit 0
fi


echo -e "\033[1;36mNumber Of Reads:\033[0m "
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^READ")\033[0m"
echo -e "\033[1;36mAverage Size Of Read:\033[0m "
echo -e "\033[1;35m$(awk 'BEGIN { FS = "[:]"} {if($1=="SENT") SUM += $2; COUNT += 1} END {print int(SUM/COUNT)}' $LOG_FL)\033[0m"
echo -e "\033[1;36mNumber Of Writes:\033[0m "
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^WRITE")\033[0m"
echo -e "\033[1;36mAverage Size Of Write:\033[0m "
echo -e "\033[1;35m$(awk 'BEGIN { FS = "[:]"} {if($1=="BYTES") SUM += $2; COUNT += 1} END {print int(SUM/COUNT)}' $LOG_FL)\033[0m"
echo -e "\033[1;36mNumber Of Locks:\033[0m"
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^LOCK")\033[0m"
echo -e "\033[1;36mNumber Of Opens:\033[0m"
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^OPEN")\033[0m"
echo -e "\033[1;36mNumber Of Unlocks:\033[0m"
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^UNLOCK")\033[0m"
echo -e "\033[1;36mNumber Of Closed Files:\033[0m"
echo -e "\033[1;35m$(tr ' ' '\n' < $LOG_FL | grep -ce "^DELETED")\033[0m"
echo -e "\033[1;36mHit Maxiumum Sise:\033[0m"
echo -e "\033[1;35m$(awk -v ma=$max 'BEGIN { FS = "[:]"} {if($1=="BYTES" || $1=="SENT"){
                                                        if($1=="BYTES"){COUNT+=$2};
                                                        if($1=="SENT"){COUNT-=$2}
                                                        if(COUNT > max){max=COUNT}
                                                    }} END {print max}' $LOG_FL)\033[0m"

echo -e "\033[1;36mHit Maxiumum Number Of Files:\033[0m"
echo -e "\033[1;35m$(awk -v ma=$max_f 'BEGIN { FS = "[:]"} {if($1=="OPEN" || $1=="VICTIM"){
                                                        if($1=="OPEN"){COUNT+=1};
                                                        if($1=="VICTIM"){COUNT-=1}
                                                        if(COUNT > max_f){max_f=COUNT}
                                                    }} END {print max_f}' $LOG_FL)\033[0m"
echo -e "\033[1;36mReplacement Algorithm  Was Called:\033[0m"
echo -e "\033[1;35m$(awk -v rep=$alg 'BEGIN { FS = "[:]"} {if($1=="VICTIM"){
                                                        alg+=1;
                                                    }} END {(alg==0)?alg=0:alg=alg; print alg}' $LOG_FL)\033[0m"

echo -e "\033[1;36mHit Maxiumum Connections:\033[0m"
echo -e "\033[1;35m$(awk -v ma=$con 'BEGIN { FS = "[:]"} {if($1=="New" || $1=="CLOSED"){
                                                        if($1=="New"){COUNT+=1};
                                                        if($1=="CLOSED"){COUNT-=1}
                                                        if(COUNT > con){con=COUNT}
                                                    }} END {print COUNT}' $LOG_FL)\033[0m"
echo -e "\033[1;36mNumber Of Requests Served Per Thread:\033[0m"

echo -e "\033[1;35m$(awk -F: '{if($1=="THREAD") print $2}' $LOG_FL | sed 's/[^0-9]*//g' | awk '{arr[$0]++}END{for (a in arr) print "Thread:" a, arr[a]}' )\033[0m"






