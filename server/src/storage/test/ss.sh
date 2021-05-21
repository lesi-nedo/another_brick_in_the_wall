#!/bin/bash



# while read line
# do
#   if [[ -z  !$(grep -e $line temp.txt) ]];then
#     echo -e "\033[1;31m$line"
#   fi
# done < del.txt

while read line
do
  grep --color=always -e "$line" not_delets.txt

done < temp2.txt