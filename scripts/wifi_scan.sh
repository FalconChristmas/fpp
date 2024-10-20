#!/usr/bin/bash

#look for wifi device names on system
iw dev | awk '$1=="Interface"{print $2}' > /tmp/wifi_devices

cat /tmp/wifi_devices | while read wifi_device || [[ -n $wifi_device ]];
do

iwlist $wifi_device scanning > /tmp/wifiscan #save scan results to a temp file
scan_ok=$(grep "$wifi_device" /tmp/wifiscan) #check if the scanning was ok
if [ -z "$scan_ok" ]; then #if scan was not ok, finish the script
    echo -n "
WIFI scanning failed.

"
    continue
fi

connected_address=$(iwconfig $wifi_device | sed -n 's/.*Access Point: \([0-9\:A-F]\{17\}\).*/\1/p')

if [ -f /tmp/ssids ]; then
    rm /tmp/ssids
fi
n_results=$(grep -c "ESSID:" /tmp/wifiscan) #save number of scanned cell
i=1
while [ "$i" -le "$n_results" ]; do
        if [ $i -lt 10 ]; then
                cell=$(echo "Cell 0$i - Address:")
        else
                cell=$(echo "Cell $i - Address:")
        fi
        j=`expr $i + 1`
        if [ $j -lt 10 ]; then
                nextcell=$(echo "Cell 0$j - Address:")
        else
                nextcell=$(echo "Cell $j - Address:")
        fi
        awk -v v1="$cell" '$0 ~ v1 {p=1}p' /tmp/wifiscan | awk -v v2="$nextcell" '$0 ~ v2 {exit}1' > /tmp/onecell #store only one cell info in a temp file

        ##################################################

        oneaddress=$(grep " Address:" /tmp/onecell | awk '{print $5}')
        onessid=$(grep "ESSID:" /tmp/onecell | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{gsub("ESSID:", "");print}')
#       onechannel=$(grep " Channel:" /tmp/onecell | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{gsub("Channel:", "");print}')
        onefrequency=$(grep " Frequency:" /tmp/onecell | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{gsub("Frequency:", "");print}')
        oneencryption=$(grep "Encryption key:" /tmp/onecell | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{gsub("Encryption key:on", "(secure)");print}' | awk '{gsub("Encryption key:off", "(open)  ");print}')
        onepower=$(grep "Quality=" /tmp/onecell | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{gsub("Quality=", "");print}' | awk -F ' Signal' '{print $1}')
        if [[ ${onepower} == *"/70"* ]] ; then
        onepower=$(echo $onepower | awk -F '/70' '{print $1}')
        onepower=$(awk -v v3=$onepower 'BEGIN{ print v3 * 10 / 7}')
        fi
        if [[ ${onepower} == *"/100"* ]] ; then
        onepower=$(echo $onepower | awk -F '/100' '{print $1}')
        fi
        onepower=${onepower%.*}
        onepower="(Signal strength: $onepower%)"
                if [ "$connected_address" == "$oneaddress" ]; then
                currentcon=" - Connected"
                else
                currentcon=""
                fi
        if [ -n "$oneaddress" ]; then
                echo "$onessid  $onefrequency $oneaddress $oneencryption $onepower $currentcon" >> /tmp/ssids
        else
                echo "$onessid  $onefrequency $oneencryption $onepower $currentcon" >> /tmp/ssids
        fi
        i=`expr $i + 1`
done
rm /tmp/onecell
awk '{printf("%5d : %s\n", NR,$0)}' /tmp/ssids > /tmp/sec_ssids #add numbers at beginning of line
grep ESSID /tmp/wifiscan | awk '{ sub(/^[ \t]+/, ""); print }' | awk '{printf("%5d : %s\n", NR,$0)}' | awk '{gsub("ESSID:", "");print}' > /tmp/ssids #generate file with only numbers and names
printf "Wifi Device: $wifi_device\n"
echo -n "Available WIFI Access Points:
"
cat /tmp/sec_ssids #show ssids list
printf "\n"

rm /tmp/ssids
rm /tmp/sec_ssids
rm /tmp/wifiscan
done
rm /tmp/wifi_devices
