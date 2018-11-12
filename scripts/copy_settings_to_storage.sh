#!/bin/bash
DEVICE=$1

FSTYPE=$(file -sL /dev/$DEVICE)

mkdir -p /tmp/smnt
if [[ "$FSTYPE" =~ "BTRFS" ]]; then
  mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "ext4" ]]; then
  mount -t ext4 -o noatime,nodiratime,nofail /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "FAT" ]]; then
  mount -t ext4 -o noatime,nodiratime,exec,nofail,flush,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "DOS" ]]; then
  mount -t auto -o noatime,nodiratime,exec,nofail,flush,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
else
  mount -t ext4 -o noatime,nodiratime,nofail /dev/$DEVICE /tmp/smnt
fi
rsync -av --modify-window=1 --exclude=music/* --exclude=sequences/* --exclude=videos/* /home/fpp/media/* /tmp/smnt
rsync -av --modify-window=1 /home/fpp/media/music /tmp/smnt
rsync -av --modify-window=1 /home/fpp/media/sequences /tmp/smnt
rsync -av --modify-window=1 /home/fpp/media/videos /tmp/smnt


umount /tmp/smnt
rmdir /tmp/smnt
