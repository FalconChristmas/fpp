#!/bin/bash
#
# Updates to bring master users in line with needs of FPP v1.5 SD images
# - Create a 'fpp' user with same uid/gid/homedir as 'pi' user
# - Install packages for OLA
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

grep -q fpp /etc/passwd
if [ $? -ne 0 ]
then
	grep "^pi:" /etc/passwd | sed -e "s/^pi:/fpp:/" >> /etc/passwd
fi

sudo apt-get update

case "${FPPPLATFORM}" in
    'BeagleBone Black')
		sudo apt-get -y --force-yes install libcppunit-dev libcppunit-1.12-1 uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev zlib1g-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev

		mkdir /tmp/deb
		cd /tmp/deb
		FILES="libola-dev_0.9.7-1_armhf.deb libola1_0.9.7-1_armhf.deb ola-python_0.9.7-1_all.deb ola-rdm-tests_0.9.7-1_all.deb ola_0.9.7-1_armhf.deb"
		for FILE in ${FILES}
		do
		    wget -nd http://www.bc2va.org/chris/tmp/fpp/deb/${FILE}
		done
		dpkg --unpack ${FILES}
		rm -f ${FILES}
	;;

    'Raspberry Pi')
		echo "deb http://apt.openlighting.org/raspbian wheezy main" > /etc/apt/sources.list.d/ola.list
		apt-get update
		apt-get -y install ola ola-rdm-tests ola-conf-plugins ola-dev libprotobuf-dev
	;;
esac

sudo apt-get -y clean

