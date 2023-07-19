#!/bin/bash
DEVICE=$1
DPATH=$2 # Folder For backups
DIRECTION=$3
RSTORAGE=$4 # Default 'none', Remote Storage Device (USB, SSD, etc) where backups will go
COMPRESS=$5 # Whether data is sent compressed to spend up network transfers
DELETE=$6
shift 6

BASEDIRECTION=$(echo $DIRECTION | cut -c1-4)
REMOTE_COMPRESS=""

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
        mount -t auto -o noatime,nodiratime,exec,nofail,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
    elif [[ "$FSTYPE" =~ "DOS" ]]; then
        EXTRA_ARGS="--no-perms"
        mount -t auto -o noatime,nodiratime,exec,nofail,uid=500,gid=500 /dev/$DEVICE /tmp/smnt
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
    # Destination is as normal will go to the specified FPP Storage Device by default
    DEST=${DEVICE}::media/backups/$DPATH

    # rsync won't create destination subdirectories for us, so create
    # a temp local subdir tree and sync it over to the remote
    mkdir -p /home/fpp/media/tmp/backups/$DPATH

    #If a remote storage device has been specified and it's not empty none (which is the default FPP storage), get the remote host to mount it so we can then copy to it
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
      then
        # Call the backup API to mount the specified device
        # Build up these variables so the can be called easier (now and unmount at end of script)
        REMOTE_MOUNT=$(curl --location --request POST -H "Content-Type:application/json" "$DEVICE/api/backups/devices/mount/$RSTORAGE/remote_filecopy")
        echo " "
        echo -n "Remote Host: $DEVICE reported..."
        echo "$REMOTE_MOUNT" | grep -Po '"Message": *\K"[^"]*"'
        echo " "
        # Remote storage device will be mounted to /mnt/remote_filecopy (at the destination), so adjust the destination to accommodate this.
        DEST=${DEVICE}::remote_filecopy/$DPATH
        # Going to a different storage device so modify the directory and copy over the base directory structure
        rsync -a /home/fpp/media/tmp/backups/* ${DEVICE}::remote_filecopy 2>&1 | IgnoreWarnings
        rm -rf /home/fpp/media/tmp/backups
    else
        # Not going to a different storage device so copy over the new directory structure as normal (this will go into the specified default FPP Storage)
        rsync -a /home/fpp/media/tmp/backups/* ${DEVICE}::media/backups/ 2>&1 | IgnoreWarnings
        rm -rf /home/fpp/media/tmp/backups
    fi

elif [ "$DIRECTION" == "FROMREMOTE" ]; then
    SOURCE=${DEVICE}::media/backups/$DPATH
    DEST=/home/fpp/media

    #If a remote storage device has been specified and it's not empty none (which is the default FPP storage), get the remote host to mount it so we can then pull from it
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
      then
        # Call the backup API to mount the specified device
        # Build up these variables so the can be called easier (now and unmount at end of script)
        REMOTE_MOUNT=$(curl --location --request POST -H "Content-Type:application/json" "$DEVICE/api/backups/devices/mount/$RSTORAGE/remote_filecopy")
        echo " "
        echo -n "Remote Host: $DEVICE reported..."
        echo "$REMOTE_MOUNT" | grep -Po '"Message": *\K"[^"]*"'
        echo " "
        # Remote storage device will be mounted to /mnt/remote_filecopy (at the destination), so adjust the source (where to pull from) to accommodate this.
        SOURCE=${DEVICE}::remote_filecopy/$DPATH
    fi

fi

EXTRA_ARGS="$EXTRA_ARGS -av --progress --info=name0 --human-readable --modify-window=1"

if [ "$COMPRESS" == "yes" ]; then
        REMOTE_COMPRESS=" -Dz "
fi

if [ "$DELETE" == "yes" ]; then
    EXTRA_ARGS="$EXTRA_ARGS --delete"
fi

for action in $@; do
    echo " "
    echo "Copying $action.... Please wait!"

    case $action in
    "All")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS --exclude=music/* --exclude=sequences/* --exclude=videos/* $SOURCE/* $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/sequences $DEST  2>&1 | IgnoreWarnings
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | IgnoreWarnings
        ;;
    "Music")
        rsync $EXTRA_ARGS $SOURCE/music $DEST  2>&1 | IgnoreWarnings
        ;;
    "Sequences")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/sequences $DEST  2>&1 | IgnoreWarnings
        ;;
    "Scripts")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/scripts $DEST  2>&1 | IgnoreWarnings
        ;;
    "Plugins")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/plugin* $DEST  2>&1 | IgnoreWarnings
        ;;
    "Images")
        rsync $EXTRA_ARGS $SOURCE/images $DEST  2>&1 | IgnoreWarnings
        ;;
    "Events")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/events $DEST  2>&1 | IgnoreWarnings
        ;;
    "Effects")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/effects $DEST  2>&1 | IgnoreWarnings
        ;;
    "Videos")
        rsync $EXTRA_ARGS $SOURCE/videos $DEST  2>&1 | IgnoreWarnings
        ;;
    "EEPROM")
        if [ ! -d "$DEST/config" ]
        then
            mkdir $DEST/config 2> /dev/null
            chown fpp.fpp $DEST/config 2> /dev/null
            chmod 775 $DEST/config 2> /dev/null
        fi
        if [ -f "$SOURCE/config/cape-eeprom.bin" ]
        then
            cp -v $SOURCE/config/cape-eeprom.bin $DEST/config/  2>&1 | IgnoreWarnings
            chown fpp.fpp $DEST/config/cape-eeprom.bin 2> /dev/null
            chmod 664 $DEST/config/cape-eeprom.bin 2> /dev/null
        else
            echo "$SOURCE/config/cape-eeprom.bin does not exist, nothing to copy."
        fi
        ;;
    "Playlists")
        rsync $EXTRA_ARGS $SOURCE/playlists $DEST  2>&1 | IgnoreWarnings
        ;;
    "Backups")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/backups $DEST  2>&1 | IgnoreWarnings
        ;;
    "JsonBackups")
        if [ ! -d "$DEST/config/backups" ]
        then
            mkdir -p $DEST/config/backups 2> /dev/null
            chown fpp.fpp $DEST/config/backups 2> /dev/null
            chmod 775 $DEST/config/backups 2> /dev/null
        fi
        rsync $EXTRA_ARGS $REMOTE_COMPRESS $SOURCE/config/backups $DEST/config  2>&1 | IgnoreWarnings
        ;;
    "Configuration")
        rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/*  --exclude=config/cape-eeprom.bin --exclude=effects/*  --exclude=events/*  --exclude=logs/*  --exclude=scripts/*  --exclude=plugin*  --exclude=playlists/*   --exclude=images/* --exclude=cache/* --exclude=backups/* $SOURCE/* $DEST  2>&1 | IgnoreWarnings
        ;;
    *)
        echo -n "Unknown action $action"
    esac
done

if [ "$DIRECTION" == "TOUSB" -o "$DIRECTION" == "FROMUSB" ]; then
    umount /tmp/smnt
    rmdir /tmp/smnt
fi

if [ "$DIRECTION" == "TOREMOTE" -o "$DIRECTION" == "FROMREMOTE" ]; then
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
        then
          #Wait 4 seconds before requesting a unmount
          sleep 4
          #Unmount the specified device from the remote location if going to or from remote host and a device has been specified
          REMOTE_UNMOUNT=$(curl --location --request POST -H "Content-Type:application/json" "$DEVICE/api/backups/devices/unmount/$RSTORAGE/remote_filecopy")
          echo " "
          echo -n  "Remote Host: $DEVICE reported..."
          echo "$REMOTE_UNMOUNT" | grep -Po '"Message": *\K"[^"]*"'
          echo " "
    fi
fi

if [ "$BASEDIRECTION" = "FROM" ]; then
    chown -R fpp:fpp /home/fpp/media
fi