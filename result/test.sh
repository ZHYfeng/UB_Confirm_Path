#!/bin/bash
python main.py json0-1.json | tee cmdline.txt
ret=$?
#
#  returns > 127 are a SIGNAL
#
if [ $ret -gt 127 ]; then
        sig=$((ret - 128))
        echo "Got SIGNAL $sig" >> ./log.log
        if [ $sig -eq $(kill -l SIGKILL) ]; then
                echo "process was killed with SIGKILL" >> ./log.log
                dmesg >> ./log.log
        fi
fi
