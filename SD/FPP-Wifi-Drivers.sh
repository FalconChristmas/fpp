#!/bin/bash

cd /opt
mkdir wifi
cd wifi
mkdir patches
cd patches

wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8723bu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8723au
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8192eu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8822bu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8188fu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8814au

shopt -s nullglob
KVERS=($(ls /usr/src/ | colrm 1 14))
shopt -u nullglob

# new kernel may not include the linker script that some of the builds below require
for i in "${KVERS[@]}"; do
    if [ ! -f "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds" ]; then
       mkdir -p "/usr/src/linux-headers-$i/arch/arm/kernel/"
       echo "/* SPDX-License-Identifier: GPL-2.0 */" > "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds"
       echo "SECTIONS {" >> "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds"
       echo "    .plt : { BYTE(0) }" >> "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds"
       echo "    .init.plt : { BYTE(0) }" >> "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds"
       echo "}" >> "/usr/src/linux-headers-$i/arch/arm/kernel/module.lds"
    fi
done

cd /opt/wifi
git clone https://github.com/pvaret/rtl8192cu-fixes
cd rtl8192cu-fixes
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_GENERIC = n/ARM_GENERIC = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/Mange/rtl8192eu-linux-driver
cd rtl8192eu-linux-driver
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/lwfinger/rtl8723bu
cd rtl8723bu
patch -p1 < /opt/wifi/patches/rtl8723bu
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/lwfinger/rtl8723au
cd rtl8723au
patch -p1 < /opt/wifi/patches/rtl8723au
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/morrownr/8812au-20210629
cd 8812au-20210629
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/morrownr/8814au
cd 8814au
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/morrownr/88x2bu
cd 88x2bu
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/morrownr/8821au-20210708
cd 8821au-20210708
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/morrownr/8821cu
cd 8821cu
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done

cd /opt/wifi
git clone https://github.com/lwfinger/rtl8188eu
cd rtl8188eu
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done
cp rtl8188eufw.bin /lib/firmware/
mkdir -p /lib/firmware/rtlwifi
cp rtl8188eufw.bin /lib/firmware/rtlwifi/

cd /opt/wifi
git clone https://github.com/kelebek333/rtl8188fu
cd rtl8188fu
patch -p1 < /opt/wifi/patches/rtl8188fu
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make
    KVER=$i ARCH=arm make install
done
ARCH=arm make installfw

rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf

cd /opt
rm -rf /opt/wifi

echo "options 8192cu rtw_power_mgnt=0 rtw_enusbss=0" > /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8188eu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8192eu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723bu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8812au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 88XXau rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8821au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8821cu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8814au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 88x2bu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options rtl8188fu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options rtl8812au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf

echo "blacklist rtl8192cu" > /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8192c_common" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtlwifi" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8xxxu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist r8188eu" >> /etc/modprobe.d/blacklist-native-wifi.conf

rm -f /etc/modprobe.d/blacklist-8192cu.conf

