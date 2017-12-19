#!/bin/sh

for i in `seq 1 46`; do
    config-pin -q P8_$i 2>/dev/null
done

for i in `seq 1 46`; do
    config-pin -q P9_$i 2>/dev/null
done

config-pin -q P9_91 2>/dev/null
config-pin -q P9_92 2>/dev/null
