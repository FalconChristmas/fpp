#!/bin/bash
DEVICE=$1
DPATH=$2
DIRECTION=$3
shift 3


FSTYPE=$(file -sL /dev/$DEVICE)
EXTRA_ARGS=""

mkdir -p /tmp/smnt
if [[ "$FSTYPE" =~ "BTRFS" ]]; then
  mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "ext4" ]]; then
  mount -t ext4 -o noatime,nodiratime,nofail /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "FAT" ]]; then
  EXTRA_ARGS="--no-perms"
  mount -t auto -o noatime,nodiratime,exec,nofail,flush,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
elif [[ "$FSTYPE" =~ "DOS" ]]; then
  EXTRA_ARGS="--no-perms"
  mount -t auto -o noatime,nodiratime,exec,nofail,flush,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
else
  mount -t ext4 -o noatime,nodiratime,nofail /dev/$DEVICE /tmp/smnt
fi

if [ "$DIRECTION" = "TO" ]; then
SOURCE=/home/fpp/media
DEST=/tmp/smnt/$DPATH
mkdir -p $DEST
else
DEST=/home/fpp/media
SOURCE=/tmp/smnt/$DPATH
fi

EXTRA_ARGS="$EXTRA_ARGS --delete -av --modify-window=1"
echo "Args: $EXTRA_ARGS"



for action in $@; do
    case $action in
    "All")
        rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/* $SOURCE/* $DEST
        rsync $EXTRA_ARGS $SOURCE/music $DEST
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST
        rsync $EXTRA_ARGS $SOURCE/videos $DEST
        ;;
    "Music")
        rsync $EXTRA_ARGS $SOURCE/music $DEST
        ;;
    "Sequences")
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST
        ;;
    "Scripts")
        rsync $EXTRA_ARGS $SOURCE/scripts $DEST
        ;;
    "Plugins")
        rsync $EXTRA_ARGS $SOURCE/plugin* $DEST
        ;;
    "Images")
        rsync $EXTRA_ARGS $SOURCE/images $DEST
        ;;
    "Events")
        rsync $EXTRA_ARGS $SOURCE/events $DEST
        ;;
    "Effects")
        rsync $EXTRA_ARGS $SOURCE/effects $DEST
        ;;
    "Videos")
        rsync $EXTRA_ARGS $SOURCE/videos $DEST
        ;;
    "Playlists")
        rsync $EXTRA_ARGS $SOURCE/playlists $DEST
        ;;
    "Configuration")
            rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/*  --exclude=effects/*  --exclude=events/*  --exclude=logs/*  --exclude=scripts/*  --exclude=plugin*  --exclude=playlists/*   --exclude=images/* --exclude=cache/* $SOURCE/* $DEST
        ;;
    *)
        echo -n "Uknown action $action"
    esac
done


umount /tmp/smnt
rmdir /tmp/smnt


if [ "$DIRECTION" = "FROM" ]; then
chown -R fpp:fpp /home/fpp/media
fi
