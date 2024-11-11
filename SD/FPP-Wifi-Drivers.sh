#!/bin/bash

cd /opt
mkdir wifi
cd wifi



export CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)

shopt -s nullglob
export KVERS=($(ls -d /usr/src/linux-headers-*rpt-rpi* | colrm 1 23))
shopt -u nullglob

# new wifi drivers take a bit of memory to compile
# Add some swap space
fallocate -l 256M /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile

cd /opt/wifi


git clone https://github.com/morrownr/88x2bu-20210702 88x2bu
cd 88x2bu
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf 88x2bu

git clone https://github.com/Mange/rtl8192eu-linux-driver
cd rtl8192eu-linux-driver
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf rtl8192eu-linux-driver

git clone https://github.com/lwfinger/rtl8723au
cd rtl8723au
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf rtl8723au

git clone https://github.com/morrownr/8812au-20210820
cd 8812au-20210820
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf  8812au-20210820

git clone https://github.com/morrownr/8814au
cd 8814au
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf 8814au

git clone https://github.com/morrownr/8821au-20210708
cd 8821au-20210708
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf 8821au-20210708

git clone https://github.com/morrownr/8821cu-20210916 8821cu
cd 8821cu
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf 8821cu

git clone https://github.com/lwfinger/rtl8188eu
cd rtl8188eu
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
cd ..
rm -rf rtl8188eu

git clone https://github.com/kelebek333/rtl8188fu
cd rtl8188fu
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
export USER_EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -Wno-error=incompatible-pointer-types"
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm MODULE_NAME=rtl8188fu make install
done
unset USER_EXTRA_CFLAGS
cd ..
rm -rf rtl8188fu

git clone https://github.com/Rick-Moba/rtl8192cu
cd rtl8192cu
export USER_EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -Wno-error=incompatible-pointer-types"
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
unset USER_EXTRA_CFLAGS
cd ..
rm -rf rtl8192cu

git clone https://github.com/lwfinger/rtl8723bu
cd rtl8723bu
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
export USER_EXTRA_CFLAGS="-DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT -DCONFIG_P2P_IPS"
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
unset USER_EXTRA_CFLAGS
cd ..
rm -rf rtl8723bu

git clone https://github.com/morrownr/rtl8852bu
cd rtl8852bu/
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
sed -i 's/#_PLATFORM_FILES/_PLATFORM_FILES/' Makefile
export USER_EXTRA_CFLAGS="-DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT -DCONFIG_P2P_IPS -DCONFIG_LITTLE_ENDIAN"
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
unset USER_EXTRA_CFLAGS
cd ..
rm -rf rtl8852bu

git clone https://github.com/lwfinger/rtl8852au
cd rtl8852au/
sed -i 's/KVER *:= $(shell uname -r)/KVER ?= $(shell uname -r)/' Makefile
sed -i 's/KSRC *:= /KSRC ?= /' Makefile
sed -i 's/CROSS_COMPILE *:=/CROSS_COMPILE ?=/' Makefile
sed -i 's/#_PLATFORM_FILES/_PLATFORM_FILES/' Makefile
export USER_EXTRA_CFLAGS="-DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT -DCONFIG_P2P_IPS -DCONFIG_LITTLE_ENDIAN"
for i in "${KVERS[@]}"; do
    KVER=$i ARCH=arm make clean
    KVER=$i ARCH=arm make -j ${CPUS}
    KVER=$i ARCH=arm make install
done
unset USER_EXTRA_CFLAGS
cd ..
rm -rf rtl8852au

cd /opt
rm -rf /opt/wifi


echo "options 8188eu.ko rtw_power_mgnt=0 rtw_enusbss=0" > /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8192eu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723au.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723bu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8812au.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8814au.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8821au.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8821cu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8852au.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8852bu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 88x2bu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8192cu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options rtl8188fu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options rtl8192cu.ko rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf

rm -f /etc/modprobe.d/blacklist-native-wifi.conf
touch /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8192cu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8192c_common" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8xxxu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist r8188eu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8188fu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl818x" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8712" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8188eu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8723bs" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8187" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist r8188eu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist r8723bs" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist r8712u" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtw88_8822bu" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtw88_8822b" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtw88_usb" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtw88_core" >> /etc/modprobe.d/blacklist-native-wifi.conf
mkdir -p /etc/fpp/wifi
cp -f /etc/modprobe.d/blacklist-native-wifi.conf /etc/fpp/wifi

echo "blacklist 8188eu" > /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8192eu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8192cu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8723au" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8723bu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8812au" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8814au" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8821au" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8821cu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8852au" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 8852bu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist 88x2bu" >> /etc/fpp/wifi/blacklist-external-wifi.conf
echo "blacklist rtl8188fu" >> /etc/fpp/wifi/blacklist-external-wifi.conf


swapoff /swapfile
rm -f /swapfile
