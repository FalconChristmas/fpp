#!/bin/bash
cpuUsageM=$(top -bn 1 | awk '{print $9}' | tail -n +8 | awk '{s+=$1} END {print s}')
cpuFreqM=$(echo "scale=0; " `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq` "/1000" | bc)
cpuTempM=$(echo "scale=1; " `cat /sys/class/thermal/thermal_zone0/temp` "/1000" | bc)

gpuTempM=$(/opt/vc/bin/vcgencmd measure_temp)
gpuTempM=${gpuTempM//\'C/}
gpuTempM=${gpuTempM//temp\=/}

memTotalM=$(cat /proc/meminfo | grep MemTotal | awk '{print $2}')
memTotal1=$memTotalM
memTotalM=$(echo "scale=1; $memTotal1 / 1024" | bc)

memUsageM=$(cat /proc/meminfo | grep MemFree | awk '{print $2}')
memUsageM=$(echo "scale=1; ($memTotal1 - $memUsageM) / 1024" | bc | sed 's/^\./0./')

memUsageP=$(echo "scale=1; (100/$memTotalM) * $memUsageM" | bc | sed 's/^\./0./')

rootTotalM=$(df -m / | grep / | awk '{print $2}')
rootTotalM=$(echo "scale=1; $rootTotalM / 1024" | bc)

rootUsageM=$(df -m / | grep / | awk '{print $3}')
rootUsageM=$(echo "scale=1; $rootUsageM / 1024" | bc | sed 's/^\./0./')

rootUsageP=$(echo "scale=1; (100/$rootTotalM) * $rootUsageM" | bc | sed 's/^\./0./')


echo "CPU Usage:  $cpuUsageM%"
echo "CPU Freq:   "$cpuFreqM"MHz"
echo "CPU Temp:   $cpuTempM°C"
echo ""
echo "GPU Temp:   $gpuTempM°C"
echo ""
echo "MEM Usage:  "$memUsageM"MB/"$memTotalM"MB ($memUsageP%)"
echo "Root Usage: "$rootUsageM"GB/"$rootTotalM"GB ($rootUsageP%)"

