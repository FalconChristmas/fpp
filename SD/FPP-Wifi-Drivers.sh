#!/bin/bash

cd /opt
mkdir wifi
cd wifi
mkdir patches
cd patches

wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8812au
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8192cu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8723bu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8723au
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8192eu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8822bu
wget https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/patches/rtl8812AU_8821AU


cd /opt/wifi
git clone https://github.com/pvaret/rtl8192cu-fixes
cd rtl8192cu-fixes
patch -p1 < /opt/wifi/patches/rtl8192cu
make
make install
make clean


cd /opt/wifi
git clone https://github.com/Mange/rtl8192eu-linux-driver
cd rtl8192eu-linux-driver
patch -p1 < /opt/wifi/patches/rtl8192eu
make
make install
make clean


cd /opt/wifi
git clone https://github.com/lwfinger/rtl8723bu
cd rtl8723bu
patch -p1 < /opt/wifi/patches/rtl8723bu
make
make install
make clean

cd /opt/wifi
git clone https://github.com/lwfinger/rtl8723au
cd rtl8723au
patch -p1 < /opt/wifi/patches/rtl8723au
make
make install
make clean

cd /opt/wifi
git clone https://github.com/abperiasamy/rtl8812AU_8821AU_linux
cd rtl8812AU_8821AU_linux
patch -p1 < /opt/wifi/patches/rtl8812AU_8821AU
#git clone https://github.com/zebulon2/rtl8812au-driver-5.2.9
#cd rtl8812au-driver-5.2.9
#patch -p1 < /opt/wifi/patches/rtl8812au
make
make install
make clean


cd /opt/wifi
git clone https://github.com/zebulon2/rtl8814au
cd rtl8814au
patch -p1 < /opt/wifi/patches/rtl8812au
make
make install
make clean

cd /opt/wifi
git clone https://github.com/FomalhautWeisszwerg/rtl8822bu
cd rtl8822bu
patch -p1 < /opt/wifi/patches/rtl8822bu
make
make install
make clean




rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf

cd /opt
rm -rf /opt/wifi

echo "options 8192cu rtw_power_mgnt=0 rtw_enusbss=0" > /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8192eu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8723bu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8812au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8821au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8814au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options 8822bu rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf
echo "options rtl8812au rtw_power_mgnt=0 rtw_enusbss=0" >> /etc/modprobe.d/wifi-disable-power-management.conf

echo "blacklist rtl8192cu" > /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8192c_common" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtlwifi" >> /etc/modprobe.d/blacklist-native-wifi.conf
echo "blacklist rtl8xxxu" >> /etc/modprobe.d/blacklist-native-wifi.conf
