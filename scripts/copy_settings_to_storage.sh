#!/bin/bash
DEVICE=$1
DPATH=$2 # Folder For backups
DIRECTION=$3
RSTORAGE=$4 # Default 'none', Remote Storage Device (USB, SSD, etc) where backups will go
COMPRESS=$5 # Whether data is sent compressed to spend up network transfers
DELETE=$6
shift 6

# Validate DIRECTION — strict enum to prevent injection of shell flags.
if ! [[ "$DIRECTION" =~ ^(TOUSB|FROMUSB|TOLOCAL|FROMLOCAL|TOREMOTE|FROMREMOTE)$ ]]; then
    echo "Invalid direction: $DIRECTION" >&2
    exit 1
fi
# Validate COMPRESS/DELETE — only yes/no.
if ! [[ "$COMPRESS" =~ ^(yes|no)$ ]]; then COMPRESS="no"; fi
if ! [[ "$DELETE" =~ ^(yes|no)$ ]]; then DELETE="no"; fi
# Validate DPATH — only allow safe backup folder names (no "..", no shell metachars).
# Empty DPATH is allowed (means root of USB/backups).
if [[ "$DPATH" == *".."* ]] || [[ "$DPATH" == *";"* ]] || [[ "$DPATH" == *"&"* ]] || [[ "$DPATH" == *"|"* ]] || [[ "$DPATH" == *"\`"* ]] || [[ "$DPATH" == *'$'* ]]; then
    echo "Invalid path: $DPATH" >&2
    exit 1
fi
if [[ -n "$DPATH" ]] && ! [[ "$DPATH" =~ ^[A-Za-z0-9._/-]*$ ]]; then
    echo "Invalid path: $DPATH" >&2
    exit 1
fi
# Validate DEVICE and RSTORAGE per direction.
# For USB directions DEVICE is a block device (sda1, mmcblk0p1, nvme0n1p1); for REMOTE it's IP/hostname.
is_block_device() { [[ "$1" =~ ^(sd[a-z][0-9]+|mmcblk[0-9]+p[0-9]+|nvme[0-9]+n[0-9]+p[0-9]+)$ ]]; }
is_host() { [[ "$1" =~ ^[A-Za-z0-9._-]+$ ]] || [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || [[ "$1" =~ ^[0-9a-fA-F:]+$ ]]; }
if [[ "$DIRECTION" == "TOUSB" || "$DIRECTION" == "FROMUSB" ]]; then
    if ! is_block_device "$DEVICE"; then
        echo "Invalid device: $DEVICE" >&2
        exit 1
    fi
elif [[ "$DIRECTION" == "TOREMOTE" || "$DIRECTION" == "FROMREMOTE" ]]; then
    if ! is_host "$DEVICE"; then
        echo "Invalid remote host: $DEVICE" >&2
        exit 1
    fi
    if [[ -n "$RSTORAGE" && "$RSTORAGE" != "none" ]] && ! is_block_device "$RSTORAGE"; then
        echo "Invalid remote storage: $RSTORAGE" >&2
        exit 1
    fi
fi
# TOLOCAL/FROMLOCAL use DPATH only, DEVICE is ignored — no check needed.

BASEDIRECTION=$(echo "$DIRECTION" | cut -c1-4)
REMOTE_COMPRESS=""
OVERALL_RC=0

IgnoreWarnings() {
    egrep -v "(failed to set|chown|attrs were not)"
}

if [ "$DIRECTION" == "TOUSB" -o "$DIRECTION" == "FROMUSB" ]; then
    FSTYPE=$(file -sL -- "/dev/$DEVICE")
    EXTRA_ARGS=""

    # FAT/exFAT have no on-disk ownership, so it must be set at mount time to match
    # the *actual* fpp uid/gid (not hardcoded) -- the fpp uid has not been consistent
    # across FPP's history (500 on older installs/images, 1000 since commit
    # f8f2f1408 for Trixie compatibility). See issue #2782.
    FPP_UID=$(id -u fpp)
    FPP_GID=$(id -g fpp)

    mkdir -p -- /tmp/smnt
    if [[ "$FSTYPE" =~ "BTRFS" ]]; then
        mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail -- "/dev/$DEVICE" /tmp/smnt
    elif [[ "$FSTYPE" =~ "ext4" ]]; then
        mount -t ext4 -o noatime,nodiratime,nofail -- "/dev/$DEVICE" /tmp/smnt
    elif [[ "$FSTYPE" =~ "FAT" ]]; then
        EXTRA_ARGS="--no-perms"
        mount -t auto -o noatime,nodiratime,exec,nofail,uid="$FPP_UID",gid="$FPP_GID" -- "/dev/$DEVICE" /tmp/smnt
    elif [[ "$FSTYPE" =~ "DOS" ]]; then
        EXTRA_ARGS="--no-perms"
        mount -t auto -o noatime,nodiratime,exec,nofail,uid="$FPP_UID",gid="$FPP_GID" -- "/dev/$DEVICE" /tmp/smnt
    else
        mount -t ext4 -o noatime,nodiratime,nofail -- "/dev/$DEVICE" /tmp/smnt
    fi
fi

if [ "$DIRECTION" == "TOUSB" ]; then
    SOURCE=/home/fpp/media
    DEST="/tmp/smnt/$DPATH"
    # Containment: ensure DEST stays under /tmp/smnt
    DEST_REAL=$(realpath -m -- "$DEST")
    if [[ "$DEST_REAL" != /tmp/smnt* ]]; then
        echo "Invalid destination path: $DPATH" >&2
        exit 1
    fi
    DEST="$DEST_REAL"
    mkdir -p -- "$DEST"
elif [ "$DIRECTION" == "FROMUSB" ]; then
    DEST=/home/fpp/media
    SOURCE="/tmp/smnt/$DPATH"
    SOURCE_REAL=$(realpath -m -- "$SOURCE")
    if [[ "$SOURCE_REAL" != /tmp/smnt* ]]; then
        echo "Invalid source path: $DPATH" >&2
        exit 1
    fi
    SOURCE="$SOURCE_REAL"
elif [ "$DIRECTION" == "TOLOCAL" ]; then
    SOURCE=/home/fpp/media
    DEST="/home/fpp/media/backups/$DPATH"
    DEST_REAL=$(realpath -m -- "$DEST")
    if [[ "$DEST_REAL" != /home/fpp/media/backups* ]]; then
        echo "Invalid destination path: $DPATH" >&2
        exit 1
    fi
    DEST="$DEST_REAL"
    mkdir -p -- "$DEST"
elif [ "$DIRECTION" == "FROMLOCAL" ]; then
    SOURCE="/home/fpp/media/backups/$DPATH"
    SOURCE_REAL=$(realpath -m -- "$SOURCE")
    if [[ "$SOURCE_REAL" != /home/fpp/media/backups* ]]; then
        echo "Invalid source path: $DPATH" >&2
        exit 1
    fi
    SOURCE="$SOURCE_REAL"
    DEST=/home/fpp/media
elif [ "$DIRECTION" == "TOREMOTE" ]; then
    SOURCE=/home/fpp/media
    # Destination is as normal will go to the specified FPP Storage Device by default
    DEST="${DEVICE}::media/backups/$DPATH"

    # rsync won't create destination subdirectories for us, so create
    # a temp local subdir tree and sync it over to the remote
    mkdir -p -- "/home/fpp/media/tmp/backups/$DPATH"

    #If a remote storage device has been specified and it's not empty none (which is the default FPP storage), get the remote host to mount it so we can then copy to it
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
      then
        # Call the backup API to mount the specified device
        # Build up these variables so the can be called easier (now and unmount at end of script)
        REMOTE_MOUNT=$(curl --location --request POST -H "Content-Type:application/json" -- "$DEVICE/api/backups/devices/mount/$RSTORAGE/remote_filecopy")
        echo " "
        echo -n "Remote Host: $DEVICE reported..."
        echo "$REMOTE_MOUNT" | grep -Po '"Message": *\K"[^"]*"'
        echo " "
        # Remote storage device will be mounted to /mnt/remote_filecopy (at the destination), so adjust the destination to accommodate this.
        DEST="${DEVICE}::remote_filecopy/$DPATH"
        # Going to a different storage device so modify the directory and copy over the base directory structure
        rsync -a -- "/home/fpp/media/tmp/backups"/* "${DEVICE}::remote_filecopy" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        rm -rf -- /home/fpp/media/tmp/backups
    else
        # Not going to a different storage device so copy over the new directory structure as normal (this will go into the specified default FPP Storage)
        rsync -a -- "/home/fpp/media/tmp/backups"/* "${DEVICE}::media/backups/" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        rm -rf -- /home/fpp/media/tmp/backups
    fi

elif [ "$DIRECTION" == "FROMREMOTE" ]; then
    SOURCE="${DEVICE}::media/backups/$DPATH"
    DEST=/home/fpp/media

    #If a remote storage device has been specified and it's not empty none (which is the default FPP storage), get the remote host to mount it so we can then pull from it
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
      then
        # Call the backup API to mount the specified device
        # Build up these variables so the can be called easier (now and unmount at end of script)
        REMOTE_MOUNT=$(curl --location --request POST -H "Content-Type:application/json" -- "$DEVICE/api/backups/devices/mount/$RSTORAGE/remote_filecopy")
        echo " "
        echo -n "Remote Host: $DEVICE reported..."
        echo "$REMOTE_MOUNT" | grep -Po '"Message": *\K"[^"]*"'
        echo " "
        # Remote storage device will be mounted to /mnt/remote_filecopy (at the destination), so adjust the source (where to pull from) to accommodate this.
        SOURCE="${DEVICE}::remote_filecopy/$DPATH"
    fi

fi

EXTRA_ARGS="$EXTRA_ARGS -av --progress --info=name0 --human-readable --modify-window=1"

if [ "$COMPRESS" == "yes" ]; then
        REMOTE_COMPRESS=" -Dz "
fi

if [ "$DELETE" == "yes" ]; then
    EXTRA_ARGS="$EXTRA_ARGS --delete"
fi

for action in "$@"; do
    echo " "
    echo "Copying $action.... Please wait!"

    case $action in
    "All")
        # Exclude backups/* so restoring a snapshot doesn't re-import the source
        # machine's own backups folder (backups-of-backups) - issue #2714
        # Exclude lost+found so restoring with path '/' from a drive root doesn't
        # drop the drive's filesystem artifacts into media/ - issue #2856
        # Fix: use trailing "/" not shell glob "/*" — "host::path/*" never expands locally and rsync rejects literal "*".
        # Fix: remove stray "--" before DEST — the initial "--" already terminated options; second "--" became a bogus local arg.
        rsync $EXTRA_ARGS $REMOTE_COMPRESS --exclude=music/* --exclude=sequences/* --exclude=videos/* --exclude=backups/* --exclude=lost+found -- "$SOURCE"/ "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        rsync $EXTRA_ARGS -- "$SOURCE/music" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/sequences" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        rsync $EXTRA_ARGS -- "$SOURCE/videos" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Music")
        rsync $EXTRA_ARGS -- "$SOURCE/music" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Sequences")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/sequences" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Scripts")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/scripts" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Plugins")
        # "plugin"* via shell glob works for local SOURCE but fails for daemon "host::path" (no local expansion).
        # Use rsync include filters for daemon sources so plugins/plugindata are matched remotely.
        if [[ "$SOURCE" == *"::"* ]]; then
            rsync $EXTRA_ARGS $REMOTE_COMPRESS --include='plugin*' --include='plugin*/**' --exclude='*' -- "$SOURCE"/ "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        else
            rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE"/plugin* "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        fi
        ;;
    "Images")
        rsync $EXTRA_ARGS -- "$SOURCE/images" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Events")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/events" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Effects")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/effects" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Videos")
        rsync $EXTRA_ARGS -- "$SOURCE/videos" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "EEPROM")
        if [ ! -d "$DEST/config" ]
        then
            mkdir -- "$DEST/config" 2> /dev/null
            chown fpp:fpp -- "$DEST/config" 2> /dev/null
            chmod 775 -- "$DEST/config" 2> /dev/null
        fi
        if [ -f "$SOURCE/config/cape-eeprom.bin" ]
        then
            if [ "$DIRECTION" == "FROMREMOTE" ] || [ "$DIRECTION" == "TOREMOTE" ]
            then
              rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/config/cape-eeprom.bin" "$DEST/config" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
            else
              cp -v -- "$SOURCE/config/cape-eeprom.bin" "$DEST/config/" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
              chown fpp:fpp -- "$DEST/config/cape-eeprom.bin" 2> /dev/null
              chmod 664 -- "$DEST/config/cape-eeprom.bin" 2> /dev/null
            fi
        else
            echo "$SOURCE/config/cape-eeprom.bin does not exist, nothing to copy."
        fi
        ;;
    "Playlists")
        rsync $EXTRA_ARGS -- "$SOURCE/playlists" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Backups")
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/backups" "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "JsonBackups")
        if [ ! -d "$DEST/config/backups" ]
        then
            mkdir -p -- "$DEST/config/backups" 2> /dev/null
            chown fpp:fpp -- "$DEST/config/backups" 2> /dev/null
            chmod 775 -- "$DEST/config/backups" 2> /dev/null
        fi
        rsync $EXTRA_ARGS $REMOTE_COMPRESS -- "$SOURCE/config/backups" "$DEST/config" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    "Configuration")
        rsync $EXTRA_ARGS --exclude=music/* --exclude=sequences/* --exclude=videos/*  --exclude=config/cape-eeprom.bin --exclude=effects/*  --exclude=events/*  --exclude=logs/*  --exclude=scripts/*  --exclude=plugin*  --exclude=playlists/*   --exclude=images/* --exclude=cache/* --exclude=backups/* --exclude=lost+found --exclude=fpp-info.json -- "$SOURCE"/ "$DEST" 2>&1 | IgnoreWarnings; rc=${PIPESTATUS[0]}; (( rc != 0 )) && OVERALL_RC=$rc
        ;;
    *)
        echo -n "Unknown action $action"
        OVERALL_RC=1
    esac
done

if [ "$DIRECTION" == "TOUSB" -o "$DIRECTION" == "FROMUSB" ]; then
    umount -- /tmp/smnt
    rmdir -- /tmp/smnt
fi

if [ "$DIRECTION" == "TOREMOTE" -o "$DIRECTION" == "FROMREMOTE" ]; then
    if [ "$RSTORAGE" != "" ]  && [ "$RSTORAGE" != "none" ]
        then
          #Wait 4 seconds before requesting a unmount
          sleep 4
          #Unmount the specified device from the remote location if going to or from remote host and a device has been specified
          REMOTE_UNMOUNT=$(curl --location --request POST -H "Content-Type:application/json" -- "$DEVICE/api/backups/devices/unmount/$RSTORAGE/remote_filecopy")
          echo " "
          echo -n  "Remote Host: $DEVICE reported..."
          echo "$REMOTE_UNMOUNT" | grep -Po '"Message": *\K"[^"]*"'
          echo " "
    fi
fi

if [ "$BASEDIRECTION" = "FROM" ]; then
    chown -R fpp:fpp -- /home/fpp/media
fi

echo " "
echo "========================================="
if [ "$OVERALL_RC" -ne 0 ]; then
    echo "BACKUP FAILED (rc=$OVERALL_RC)"
    echo "One or more items failed to copy - see errors above."
    echo "Check fppd.log and logs/fpp_backup_filecopy.log for details."
else
    echo "BACKUP COMPLETE"
    echo "All selected items have been successfully copied."
fi
echo "You may now close this window."
echo "Safe to remove storage device if applicable."
echo "========================================="
echo " "
exit $OVERALL_RC
