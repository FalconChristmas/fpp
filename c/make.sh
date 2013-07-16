#!/bin/bash
echo Compiling FPPD ...
gcc -o fppd fppd.c E131.c mpg123.c pixelnetDMX.c playList.c schedule.c command.c -w -l pthread -l wiringPi
echo Compiling FPP
gcc -o fpp fpp.c
