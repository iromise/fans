#!/bin/bash
while true
do 
    procnum=`ps -ef|grep "adbd"|grep -v grep|grep -v restart|wc -l`
    if [ $procnum -eq 0 ]; then
        adbd&
    fi
    sleep 30
done