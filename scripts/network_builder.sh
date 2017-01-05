#!/bin/bash
##############################################################################

BINDIR=$(dirname $0)
. ${BINDIR}/common
. ${BINDIR}/functions

logOutput

writeDnsmasq () {
	IFACE=${1}
	ROUTER=${2}
	START=${3}
	END=${4}
	MASK=${5}
	cat <<-EOF > ${CFGDIR}/dnsmasq.global
		interface=${IFACE}
		dhcp-authoritative
		dhcp-range=${START},${END},${MASK},12h
		dhcp-option=option:router,${ROUTER}
		dhcp-option=option:ntp-server,${ROUTER}
		dhcp-leasefile=/home/fpp/media/config/leases.global
	EOF
}

writeHostap () {
	IFACE=${1}
	SSID="${2}"
	PASSPHRASE="${3}"
	cat <<-EOF > ${CFGDIR}/hostapd.wlan1
		interface=${IFACE}
		driver=nl80211
		ctrl_interface=/var/run/hostapd.${IFACE}
		ssid=${SSID}
		country_code=us
		ieee80211d=1
		hw_mode=g
		ieee80211n=1
		wmm_enabled=1
		channel=6
		wpa=2
		wpa_passphrase=${PASSPHRASE}
		wpa_key_mgmt=WPA-PSK
		wpa_pairwise=CCMP
	EOF
}

writeIptables () {
	IFACE=${1}
	SOURCE_IP=${2}
	DEST_IP=${3}
	SOURCE_PORT=${4}
	DEST_PORT=${5}
	cat <<-EOF > ${CFGDIR}/iptables.global
		*nat
		:PREROUTING ACCEPT [0:0]
		:INPUT ACCEPT [0:0]
		:OUTPUT ACCEPT [0:0]
		:POSTROUTING ACCEPT [0:0]
		-A PREROUTING -i ${IFACE} -d ${SOURCE_IP}/32 -p tcp -m tcp --dport ${SOURCE_PORT} -j DNAT --to-destination ${DEST_IP}:${DEST_PORT}
		-A PREROUTING -i ${IFACE} -d ${SOURCE_IP}/32 -j DNAT --to-destination ${DEST_IP}
		-A POSTROUTING -o ${IFACE} -d ${DEST_IP}/32 -p tcp -m tcp --dport ${SOURCE_PORT} -j SNAT --to-source ${SOURCE_IP}:${SOURCE_PORT}
		-A POSTROUTING -o ${IFACE} -d ${DEST_IP}/32 -j SNAT --to-source ${SOURCE_IP}
		COMMIT
	EOF
}

writeOspf () {
	NAME=${1}
	cat <<-EOF > ${CFGDIR}/zebra.global
		!
		!
		!
		hostname ${NAME}
		password zebra
		log stdout
		!
		interface lo
		 link-detect
		!
		interface eth0
		 link-detect
		!
		interface wlan0
		 link-detect
		!
		interface wlan1
		 link-detect
		!
		line vty
		!
	EOF
	cat <<-EOF > ${CFGDIR}/ospfd.global
		!
		!
		!
		hostname ${NAME}
		password zebra
		log stdout
		!
		interface lo
		!
		interface eth0
		!
		interface wlan0
		!
		interface wlan1
		!
		router ospf
		 redistribute connected
		 network 0.0.0.0/0 area 0.0.0.0
		!
		line vty
		!
	EOF
}

writePim () {
	cat <<-EOF > ${CFGDIR}/pimd.global
		cand_rp time 30 priority 20
		cand_bootstrap_router priority 5
		group_prefix 224.0.0.0 masklen 4
		switch_data_threshold rate 50000 interval 20
		switch_register_threshold rate 50000 interval 20
	EOF
}

writeDns () {
	DNS1=${1}
	DNS2=${2}
	cat <<-EOF > ${CFGDIR}/dns.global
		DNS1="${DNS1}"
		DNS2="${DNS2}"
	EOF
}

writeWiredIf () {
	IFACE=${1}
	PROTO=${2}
	ADDRESS=${3}
	NETMASK=${4}
	GATEWAY=${5}
	cat <<-EOF > ${CFGDIR}/interface.${IFACE}
		INTERFACE="${IFACE}"
		PROTO="${PROTO}"
	EOF
	if [ "${PROTO}" == "static" ]; then
		cat <<-EOF >> ${CFGDIR}/interface.${IFACE}
			ADDRESS="${ADDRESS}"
			NETMASK="${NETMASK}"
			GATEWAY="${GATEWAY}"
		EOF
	fi
}

writeWifiIf () {
	IFACE=${1}
	PROTO=${2}
	SSID="${3}"
	PASSPHRASE="${4}"
	ADDRESS=${5}
	NETMASK=${6}
	GATEWAY=${7}
	cat <<-EOF > ${CFGDIR}/interface.${IFACE}
		INTERFACE="${IFACE}"
		PROTO="${PROTO}"
	EOF
	if [ "${PROTO}" == "static" ]; then
		cat <<-EOF >> ${CFGDIR}/interface.${IFACE}
			ADDRESS="${ADDRESS}"
			NETMASK="${NETMASK}"
			GATEWAY="${GATEWAY}"
		EOF
	fi
	cat <<-EOF >> ${CFGDIR}/interface.${IFACE}
		SSID="${ADDRESS}"
		PSK="${PASSPHRASE}"
	EOF
}

writeSetting () {
	awk -f ${FPPDIR}/scripts/writeSetting.awk ${SETTINGSFILE} setting=EnableRouting value=1
}

readSetting () {
	awk -f ${FPPDIR}/scripts/readSetting.awk ${SETTINGSFILE} setting=EnableRouting
}
