#!/bin/bash
temps=()
# if [[ $# -lt 2 ]]; then 
#   echo "usage: file1 file2 [...]";
#   exit 0;
# fi

awk -F ': ' '{print $3}' storage.txt > temp2.txt
awk -F ': ' '{print $2}' storage.txt | awk -F ' ' '{print $1}' > temp.txt

echo -e "\033[1;34mIf you see only this than diff between files in cache and in storage went well.\033[0;m"
sort temp.txt > t.txt && cat t.txt > temp.txt && rm t.txt
sort temp2.txt > t1.txt && cat t1.txt > temp2.txt && rm t1.txt
diff temp.txt temp2.txt


# for i in "$@"; do
#   sort "$i" > temp_"${i}";
#   temps+=(temp_"${i}");
# done

# for (( i=0; i<=${#temps[@]}-1; i++ )) do
#   for var in ${temps[@]:$i + 1:${#temps[@]} - 1}; do
#     printf "\033[1;36m diff %s %s\033[0;37m\n" ${temps[$i]:5} ${var:5};
#     printf "\033[1;34m";
#     diff ${temps[$i]} $var;
#   done
# done

# rm ${temps[@]};
