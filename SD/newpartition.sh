#!/bin/bash

LASTBLOCK=$(sfdisk -q -l -o sectors /dev/mmcblk0 2>/dev/null | tail -n 1)
if [ "$LASTBLOCK" != "" ]; then
    LASTBLOCK=$(($LASTBLOCK + 16))
    echo "${LASTBLOCK},,L,*" | sfdisk --force --no-reread --append /dev/mmcblk0
fi

