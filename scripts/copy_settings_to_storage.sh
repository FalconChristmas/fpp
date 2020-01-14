#!/bin/bash
DEVICE=$1
DPATH=$2
DIRECTION=$3
shift 3

BASEDIRECTION=$(echo $DIRECTION | cut -c1-4)

if [ "$DIRECTION" == "TOUSB" -o "$DIRECTION" == "FROMUSB" ]; then
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
fi

if [ "$DIRECTION" == "TOUSB" ]; then
    SOURCE=/home/fpp/media
    DEST=/tmp/smnt/$DPATH
    mkdir -p $DEST
elif [ "$DIRECTION" == "FROMUSB" ]; then
    DEST=/home/fpp/media
    SOURCE=/tmp/smnt/$DPATH
elif [ "$DIRECTION" == "TOLOCAL" ]; then
    SOURCE=/home/fpp/media
    DEST=/home/fpp/media/backups/$DPATH
    mkdir -p $DEST
elif [ "$DIRECTION" == "FROMLOCAL" ]; then
    SOURCE=/home/fpp/media/backups/$DPATH
    DEST=/home/fpp/media
elif [ "$DIRECTION" == "TOREMOTE" ]; then
    SOURCE=/home/fpp/media
    DEST=${DEVICE}::media/backups/$DPATH
    # rsync won't create destination subdirectories for us, so create
    # a temp local subdir tree and sync it over to the remote
    mkdir -p /home/fpp/media/tmp/backups/$DPATH
    rsync -a /home/fpp/media/tmp/backups/* ${DEVICE}::media/backups/ 2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
    rm -rf /home/fpp/media/tmp/backups
elif [ "$DIRECTION" == "FROMREMOTE" ]; then
    SOURCE=${DEVICE}::media/backups/$DPATH
    DEST=/home/fpp/media
fi

EXTRA_ARGS="$EXTRA_ARGS --delete -av --modify-window=1"


for action in $@; do
    case $action in
    "All")
        rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/* $SOURCE/* $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Music")
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Sequences")
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Scripts")
        rsync $EXTRA_ARGS $SOURCE/scripts $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Plugins")
        rsync $EXTRA_ARGS $SOURCE/plugin* $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Images")
        rsync $EXTRA_ARGS $SOURCE/images $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Events")
        rsync $EXTRA_ARGS $SOURCE/events $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Effects")
        rsync $EXTRA_ARGS $SOURCE/effects $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Videos")
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Playlists")
        rsync $EXTRA_ARGS $SOURCE/playlists $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Backups")
        rsync $EXTRA_ARGS $SOURCE/backups $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    "Configuration")
        rsync $EXTRA_ARGS -q --exclude=music/* --exclude=sequences/* --exclude=videos/*  --exclude=effects/*  --exclude=events/*  --exclude=logs/*  --exclude=scripts/*  --exclude=plugin*  --exclude=playlists/*   --exclude=images/* --exclude=cache/* --exclude=backups/* $SOURCE/* $DEST  2>&1 | grep -v "failed to set" | grep -v chown | grep -v "attrs were not"
        ;;
    *)
        echo -n "Uknown action $action"
    esac
done

if [ "$DIRECTION" == "TOUSB" -o "$DIRECTION" == "FROMUSB" ]; then
    umount /tmp/smnt
    rmdir /tmp/smnt
fi

if [ "$BASEDIRECTION" = "FROM" ]; then
    chown -R fpp:fpp /home/fpp/media
fi

