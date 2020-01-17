#!/bin/bash
DEVICE=$1
DPATH=$2
DIRECTION=$3
DELETE=$4
shift 4

BASEDIRECTION=$(echo $DIRECTION | cut -c1-4)

IgnoreWarnings() {
    egrep -v "(failed to set|chown|attrs were not)"
}

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
    rsync -a /home/fpp/media/tmp/backups/* ${DEVICE}::media/backups/ 2>&1 | IgnoreWarnings
    rm -rf /home/fpp/media/tmp/backups
elif [ "$DIRECTION" == "FROMREMOTE" ]; then
    SOURCE=${DEVICE}::media/backups/$DPATH
    DEST=/home/fpp/media
fi

EXTRA_ARGS="$EXTRA_ARGS -av --modify-window=1"

if [ "$DELETE" == "yes" ]; then
    EXTRA_ARGS="$EXTRA_ARGS --delete"
fi

for action in $@; do
    case $action in
    "All")
        rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/* $SOURCE/* $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | IgnoreWarnings
        ;;
    "Music")
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | IgnoreWarnings
        ;;
    "Sequences")
        rsync $EXTRA_ARGS $SOURCE/sequences $DEST  2>&1 | IgnoreWarnings
        ;;
    "Scripts")
        rsync $EXTRA_ARGS $SOURCE/scripts $DEST  2>&1 | IgnoreWarnings
        ;;
    "Plugins")
        rsync $EXTRA_ARGS $SOURCE/plugin* $DEST  2>&1 | IgnoreWarnings
        ;;
    "Images")
        rsync $EXTRA_ARGS $SOURCE/images $DEST  2>&1 | IgnoreWarnings
        ;;
    "Events")
        rsync $EXTRA_ARGS $SOURCE/events $DEST  2>&1 | IgnoreWarnings
        ;;
    "Effects")
        rsync $EXTRA_ARGS $SOURCE/effects $DEST  2>&1 | IgnoreWarnings
        ;;
    "Videos")
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | IgnoreWarnings
        ;;
    "Playlists")
        rsync $EXTRA_ARGS $SOURCE/playlists $DEST  2>&1 | IgnoreWarnings
        ;;
    "Backups")
        rsync $EXTRA_ARGS $SOURCE/backups $DEST  2>&1 | IgnoreWarnings
        ;;
    "Configuration")
        rsync $EXTRA_ARGS -q --exclude=music/* --exclude=sequences/* --exclude=videos/*  --exclude=effects/*  --exclude=events/*  --exclude=logs/*  --exclude=scripts/*  --exclude=plugin*  --exclude=playlists/*   --exclude=images/* --exclude=cache/* --exclude=backups/* $SOURCE/* $DEST  2>&1 | IgnoreWarnings
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

