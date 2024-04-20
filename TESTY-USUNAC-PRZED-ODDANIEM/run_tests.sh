#!/bin/bash

VG_FLAGS="--error-exitcode=123 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes"

num_tests=10
vg=0

while getopts 'vn:' OPTION; do

    case "$OPTION" in
        v)
            vg=1
            ;;
        n)
            num_tests=$(expr "$OPTARG" + 0)
            ;;
        ?)
            echo "Script usage: $(basename \$0) [-v] [-n number of tests]"
            exit 1
            ;;
    esac
done


if ! make libnand.so; then
    echo -e "\e[31mCompilation of libnand.so failed! Aborting...\e[0m"
    exit
fi

if ! g++ test_gen.cpp -std=c++17 -o test_gen; then
    echo -e "\e[31mCompilation of test_gen failed! Aborting...\e[0m"
    exit
fi

if ! gcc -L. -o auto interactor.c libnand.so -lnand; then
    echo -e "\e[31mCompilation of interactor failed! Aborting...\e[0m"
    exit
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
    
for((i = 1; i <= $num_tests; i++)); do

    echo -ne "Test $i: "
    ./test_gen $i > test.in
    
    if(($? != 0)); then
        echo -e "\e[33mTest gen runtime error\e[0m"
        break
    fi
    
    if(($vg == 0)); then
        ./auto < test.in > auto.out
    else
        valgrind $VG_FLAGS ./auto < test.in > auto.out
    fi
    
    if(($? != 0)); then
        echo -e "\e[33mRuntime error\e[0m"
        break
    fi
    
    ./brute < test.in > brute.out
    
    if(($? != 0)); then
        echo -e "\e[33mBrute runtime error\e[0m"
        break
    fi
    
    if ! diff -wq auto.out brute.out; then
        echo -e "\e[31mThe files differ\e[0m"
        break
    else
        echo -e "\e[32mOK\e[0m"
    fi
    
done
