#!/bin/sh

BASE=http://localhost:32322/fppd

SECTION="None"
if [ "x$1" != "x" ]
then
	SECTION=$1
fi

echo "Testing section: ${SECTION}"

if [ "x${SECTION}" == "xlog" -o "x${SECTION}" == "xall" ]
then
	curl ${BASE}/log
	curl -d "" ${BASE}/log/level/debug
	curl -d "" ${BASE}/log/mask/http,channelout
fi

if [ "x${SECTION}" == "xoutputs" -o "x${SECTION}" == "xall" ]
then
	curl -d "{ \"command\": \"disable\" }" ${BASE}/outputs
	curl -d "{ \"command\": \"enable\" }" ${BASE}/outputs
fi

if [ "x${SECTION}" == "xschedule" -o "x${SECTION}" == "xall" ]
then
	curl -d "{ \"command\": \"reload\" }" ${BASE}/schedule
fi

if [ "x${SECTION}" == "xshutdown" -o "x${SECTION}" == "xall" ]
then
	curl -d "" ${BASE}/shutdown
fi

if [ "x${SECTION}" == "xtesting" -o "x${SECTION}" == "xall" ]
then
	curl ${BASE}/testing
	curl -d "{\"enabled\":0}" ${BASE}/testing
fi

if [ "x${SECTION}" == "xversion" -o "x${SECTION}" == "xall" ]
then
	curl ${BASE}/version
fi

if [ "x${SECTION}" == "xvolume" -o "x${SECTION}" == "xall" ]
then
	curl ${BASE}/volume
	curl -d "" ${BASE}/volume/73
fi

