<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script type="text/javascript" src="jquery/jQuery.msgBox/scripts/jquery.msgBox.js?ref=<?= filemtime('jquery/jQuery.msgBox/scripts/jquery.msgBox.js'); ?>"></script>
<link href="jquery/jQuery.msgBox/styles/msgBoxLight.css?ref=<?= filemtime('jquery/jQuery.msgBox/styles/msgBoxLight.css'); ?>" rel="stylesheet" type="text/css">

<script>
    function StorageDialogDone() {
        EnableModalDialogCloseButton("storageSettingsProgress");
        SetRebootFlag();
    }
    function growSDCardFS() {
        DisplayConfirmationDialog("growSDCard", "Grow Filesystem", $("#dialog-confirm"), function () {
            DisplayProgressDialog("storageSettingsProgress", "Storage Expand");
            StreamURL('growsd.php?wrapped=1', 'storageSettingsProgressText', 'StorageDialogDone');
        });
    }
    function newSDCardPartition() {
        DisplayConfirmationDialog("growSDCard", "Grow Filesystem", $("#dialog-confirm-newpartition"), function () {
            DisplayProgressDialog("storageSettingsProgress", "Storage Expand");
            StreamURL('newpartitionsd.php?wrapped=1', 'storageSettingsProgressText', 'StorageDialogDone');
        });
    }

    function checkForStorageCopy() {
        $.msgBox({
            title: "Copy settings?",
            content: "Would you like to copy all files to the new storage location?\nAll settings on the new storage will be overwritten.",
            type: "info",
            buttons: [{ value: "Yes" }, { value: "No" }],
            success: function (result) {
                if (result == "Yes") {
                    DisplayProgressDialog("storageSettingsProgress", "Storage Expand");
                    StreamURL("copystorage.php?wrapped=1&storageLocation=" + $('#storageDevice').val() + "&direction=TOUSB&delete=no&path=/&flags=All", 'storageSettingsProgressText', 'StorageDialogDone');
                }
            }
        });
    }

    function checkFormatStorage() {
        var value = $('#storageDevice').val();

        var e = document.getElementById("storageDevice");
        var name = e.options[e.selectedIndex].text;
        if (name.includes("Not Mounted")) {
            var btitle = "Format Storage Location (" + value + ")" + name;
            $.msgBox({
                type: "prompt",
                title: btitle,
                inputs: [
                    { header: "Don't Format", type: "radio", name: "formatType", checked: "", value: "none" },
                    { header: "ext4 (Most stable)", type: "radio", name: "formatType", value: "ext4" },
                    { header: "exFAT (Compatible with Windows/OSX, experimental, not recommended)", type: "radio", name: "formatType", value: "exFAT" },
                    { header: "FAT (Compatible with Windows/OSX, unsupported, slow, not recommended)", type: "radio", name: "formatType", value: "FAT" }
                ],
                buttons: [{ value: "OK" }],
                opacity: 0.5,
                success: function (result, values) {
                    var v = $('input[name=formatType]:checked').val();
                    if (v != "none") {
                        $.ajax({
                            url: "formatstorage.php?fs=" + v + "&storageLocation=" + $('#storageDevice').val(),
                            async: false,
                            success: function (data) {
                                checkForStorageCopy();
                            },
                            failure: function (data) {
                                DialogError("Format Storage", "Error formatting storage.");
                            }
                        });
                    } else {
                        checkForStorageCopy();
                    }
                }
            });
        }
    }

    // One entry point for every platform and every target.  flash_storage.sh works
    // out the partition layout and boot configuration for the hardware it is on.
    function flashStorage(device, mode, label, btrfs) {
        var title = (mode == 'copy' ? 'Copy FPP to ' : 'Create new FPP on ') + label;

        $('#flash-target-name').text(label + ' - /dev/' + device);
        DisplayConfirmationDialog("flashStorage", title, $("#dialog-confirm-flash"), function () {
            DisplayProgressDialog("flashStorageProgress", title);
            var url = "flash-storage.php?dev=" + encodeURIComponent(device) + "&mode=" + encodeURIComponent(mode);
            if (btrfs) {
                url += "&btrfs=true";
            }
            StreamURL(url, 'flashStorageProgressText', 'ProgressDialogDone', 'ProgressDialogDone');
        });
    }

    function unmountUSBDevice(usbDevice, mountLocation) {
        $.post("api/backups/devices/unmount/" + usbDevice + "/" + mountLocation).done(function (data) {
            $('#unmount_' + usbDevice).remove();
        });
    }
</script>

<?php

function PrintStorageDeviceSelect($platform)
{
    global $SUDO;

    //exec('lsblk -l | grep ' . GetDirSetting('boot') . ' | cut -f1 -d" " | sed -e "s/p[0-9]$//"', $output, $return_val);
    exec('lsblk -l | grep ' . GetDirSetting('boot') . ' | cut -f1 -d" "', $output, $return_val);
    if (count($output) > 0) {
        $bootDevice = $output[0];
    } else {
        $bootDevice = "";
    }
    unset($output);

    if ($platform == "BeagleBone Black") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
        $rootDevice = isset($output[0]) ? $output[0] : "";
        unset($output);

        if ($bootDevice == "") {
            exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
            $bootDevice = isset($output[0]) ? $output[0] : "";
            unset($output);
        }
    } else {
        exec('lsblk -l | grep " /$" | cut -f1 -d" "', $output, $return_val);
        $rootDevice = isset($output[0]) ? $output[0] : "";
        unset($output);
    }

    $storageDevice = "";
    exec('findmnt -no source -T ' . GetSettingValue('mediaDirectory') . '  | sed -e "s/\/dev\///"', $output, $return_val);
    if (isset($output[0]))
        $storageDevice = $output[0];
    unset($output);

    $found = 0;
    $values = array();

    foreach (scandir("/dev/") as $fileName) {
        if (
            (preg_match("/^sd[a-z][0-9]/", $fileName)) ||
            (preg_match("/^mmcblk[0-9]p[0-9]/", $fileName)) ||
            (preg_match("/^nvme[0-9]n[0-9]p[0-9]/", $fileName))
        ) {
            exec($SUDO . " sfdisk -s /dev/$fileName", $output, $return_val);
            $GB = intval($output[0]) / 1024.0 / 1024.0;
            unset($output);

            if ($GB <= 0.1)
                continue;

            $FreeGB = "Not Mounted";
            exec("df -k /dev/$fileName | grep $fileName | awk '{print $4}'", $output, $return_val);
            if (count($output)) {
                $FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
                unset($output);
            } else {
                unset($output);

                if (preg_match("/^$rootDevice/", $fileName)) {
                    exec("df -k / | grep ' /$' | awk '{print \$4}'", $output, $return_val);
                    if (count($output))
                        $FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
                    unset($output);
                }
            }

            $key = $fileName . " ";
            $type = "";

            if (preg_match("/^$storageDevice/", $fileName)) {
                $type .= " (current storage device)";
            }

            if (preg_match("/^$bootDevice/", $fileName)) {
                $type .= " (boot device)";
            }

            if (preg_match("/^sd/", $fileName)) {
                $type .= " (USB)";
            }

            $key = sprintf("%s - %.1fGB (%s) %s", $fileName, $GB, $FreeGB, $type);

            $values[$key] = $fileName;

            if ($storageDevice == $fileName)
                $found = 1;
        }
    }

    if (!$found) {
        $arr = array_reverse($values, true);
        $values = $arr;
    }
    if ($storageDevice == "") {
        $storageDevice = $rootDevice;
    }

    PrintSettingSelect('StorageDevice', 'storageDevice', 0, 1, $storageDevice, $values, "", "checkFormatStorage");
}


$addnewfsbutton = false;
exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
$rootDevice = $output[0];
if ($rootDevice == 'mmcblk0p1' || $rootDevice == 'mmcblk0p2' || $rootDevice == 'mmcblk0p3' || $rootDevice == 'nvme0n1p2' || $rootDevice == 'sda2') {
    if (isset($settings["UnpartitionedSpace"]) && $settings['UnpartitionedSpace'] > 0) {
        $addnewfsbutton = true;
    }
}

// Every disk that is neither the booted one nor the media drive can be flashed, on
// every platform.  No per-platform lists: GetFlashTargetDevices() decides, and
// flash_storage.sh picks the right layout for the platform it is running on.
$flashTargets = GetFlashTargetDevices();

// BTRFS is a BeagleBone-only root filesystem option, and not on the PocketBeagle2.
$offerBtrfs = ($settings['Platform'] == "BeagleBone Black")
    && (strpos($settings['SubPlatform'], 'PocketBeagle') === FALSE);

if ($addnewfsbutton) {
    ?>
    <br>
    <h3>SD Card Actions:</h3>
    <div class="row">
        <div class="col-md-2"><input style='width:13em;' type='button' class='buttons' value='Grow Filesystem'
                onClick='growSDCardFS();'></div>
        <div class="col-md-10">This will grow the file system on the SD card to use the entire size of the SD card.</div>
    </div>

    <? if ($uiLevel >= 1) { ?>
        <div class="row mt-2">
            <div class="col-md-2"><input style='width:13em;' type='button' class='buttons' value='New Partition'
                    onClick='newSDCardPartition();'></div>
            <div class="col-md-10"><b>*</b>&nbsp;This will create a new partition in the unused aread of the SD card. The new
                partition can be selected as a storage location and formatted to BTRFS or ext4 after a reboot.
            </div>
        </div>
    <? } ?>

    <hr class="mt-2 mb-2">
    <?php
}
if (!empty($flashTargets)) {
    ?>
    <h3>Flash FPP to Another Device:</h3>
    <p>
        <b>Create</b> writes a clean FPP install, leaving your media, sequences and settings behind.
        <b>Copy</b> does the same but brings them along.
        Both give the copy its own ssh host keys, so it can safely run as a separate player
        alongside this one. Either way, everything currently on the target device is erased.
    </p>
    <?php foreach ($flashTargets as $flashTarget) {
        $jsName = htmlspecialchars(json_encode($flashTarget['name']), ENT_QUOTES);
        $jsDesc = htmlspecialchars(json_encode($flashTarget['desc']), ENT_QUOTES);
        ?>
        <div class="row align-items-center mt-2">
            <div class="col-md-4">
                <span class="fw-semibold"><?php echo htmlspecialchars($flashTarget['desc']); ?></span>
                <small class="text-body-secondary d-block">/dev/<?php echo htmlspecialchars($flashTarget['name']); ?></small>
            </div>
            <div class="col-auto">
                <input type="button" class="buttons" value="Create"
                    onClick="flashStorage(<?php echo $jsName; ?>, 'create', <?php echo $jsDesc; ?>)">
            </div>
            <div class="col-auto">
                <input type="button" class="buttons" value="Copy"
                    onClick="flashStorage(<?php echo $jsName; ?>, 'copy', <?php echo $jsDesc; ?>)">
            </div>
            <?php if ($offerBtrfs && $uiLevel >= 1 && $flashTarget['kind'] == 'eMMC') { ?>
                <div class="col-auto">
                    <input type="button" class="buttons" value="Create (BTRFS)"
                        onClick="flashStorage(<?php echo $jsName; ?>, 'create', <?php echo $jsDesc; ?>, true)">
                    <i class="fas fa-fw fa-graduation-cap ui-level-1"></i>
                    <small class="text-body-secondary">BTRFS compresses the root filesystem to save space on the
                        eMMC, at the cost of extra CPU.</small>
                </div>
            <?php } ?>
        </div>
    <?php } ?>
    <hr class="mt-3 mb-2">
    <?php
}

if ($settings['Platform'] != "Docker") { ?>
    <br><br>
    <b>Storage Device:</b> &nbsp;<? PrintStorageDeviceSelect($settings['Platform']); ?>

    <? if (
        strpos($settings['SubPlatform'], "Raspberry Pi 4") !== false ||
        strpos($settings['SubPlatform'], "Raspberry Pi 5") !== false ||
        strpos($settings['SubPlatform'], "Raspberry Pi Compute Module 5") !== false
    ) { ?>

        <div class="callout callout-warning">
            Changing the storage device to USB devices is strongly discouraged. There are all kinds of
            problems that using USB storage introduce into the system which can easily result in various problems include
            network lag, packet drops, audio clicks/pops, high CPU usage, etc... Using USB storage also results in longer bootup
            time. In addition, many advanced features and various capes/hats are known to NOT work when using USB storage.
            <br><br>
            In addition to the above, since it is not recommended, using NVMe/USB storage is not tested nearly as extensively by
            the
            FPP developers. Thus, upgrades (even "patch" upgrades) have a higher risk of unexpected problems. By selecting a
            NVMe/USB
            storage device, you assume much higher risk of problems and issues than when selecting an SD partition.
        </div>
        <?php
        // This warning used to sit in the else branch below, which a Pi 5 can never
        // reach because it already matched the condition above -- so it never once
        // displayed on the hardware it is about.
        if (
            strpos($settings['SubPlatform'], "Raspberry Pi 5") !== false ||
            strpos($settings['SubPlatform'], "Raspberry Pi Compute Module 5") !== false
        ) { ?>
            <div class="callout callout-warning">
                <b>Power:</b> a Raspberry Pi 5 limits its USB ports to 600mA in total unless it detects a 5A (27W)
                supply. That is not enough for many SSDs and drive enclosures &mdash; they brown out and reset
                mid-transfer, or never appear at all, and the Pi will not boot from them. Use the 27W adapter.
                Flashing to a USB device adds <code>usb_max_current_enabled=1</code> to the new install so it can
                draw the full 1.6A; only run that with an adequate supply.
            </div>
        <? } ?>
    <? } else { ?>
        <div class="callout callout-warning">
            If using a USB storage device, it is STRONGLY recommended that the device be a USB 3.0 SATA/SSD device or other fast
            storage and not a generic USB Thumb drive. Older USB devices, even on the USB 3.0 ports, are known to cause all
            kinds of problems including network lag, packet drops, audio clicks/pops, high CPU usage, etc...
            <br><br>
            In addition, a good cooling solution, particularly for the USB HUB chips on the Pi, is critical. It is recommended
            to have a cooling fan and heat sinks on the Pi4/5 chips to keep everything cool. When the chips get too hot, the
            entire system is throttled which introduces latency and lag.
        </div>
        <br>
        <?
    }
}
?>

<div id="dialog-confirm" class="hidden">
    <p><i class="fas fa-exclamation-triangle text-danger me-2"></i>Growing the filesystem will
        require a reboot to take effect. Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-flash" class="hidden">
    <p><i class="fas fa-exclamation-triangle text-danger me-2"></i>This will <b>erase everything</b> on:</p>
    <p class="fw-semibold" id="flash-target-name"></p>
    <p>Flashing can take a long time. Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-newpartition" class="hidden">
    <p><i class="fas fa-exclamation-triangle text-danger me-2"></i>Creating a new partition in
        the unused space will require a reboot to take effect. Do you wish to proceed?</p>
</div>

<hr>
<h3>Mounted USB Device Actions:</h3>

<?php
$systems_UsbDevices = GetAvailableBackupsDevices(true);
$foundAMountedDevice = false;
if (!empty($systems_UsbDevices)) {
    foreach ($systems_UsbDevices as $usbID => $usbInfo) {
        $usbName = trim($usbInfo['name']);
        $usbModel = $usbInfo['model'];
        $usbVendor = $usbInfo['vendor'];
        $usbSize = $usbInfo['size'] . " GB";
        $usbMountLocation = "";

        $deviceIsUsable = CheckIfDeviceIsUsable($usbName);
        //CheckIfDeviceIsUsable returns a empty string if device is usable
        if ($deviceIsUsable != '') {
            $foundAMountedDevice = true;
            //find where mounted
            $usbMountLocation = shell_exec('findmnt -nr -o target -S /dev/' . $usbName);
            $usbMountLocation_folder = trim(str_replace("/mnt/", "", $usbMountLocation));
            $onClick_unmount = "unmountUSBDevice(\"$usbName\",\"$usbMountLocation_folder\");";

            //Find what files are open for the mount
            $openFiles = trim(shell_exec("sudo lsof +f -- /dev/$usbName"));
            ?>
            <div id="unmount_<?php echo $usbName; ?>" class="row">
                <div id="mounted-usb-info" class="row">
                    <div id="mounted-usb-name_<?php echo $usbName; ?>" class="col-md-3">
                        <?php echo $usbName . " - " . $usbVendor . " - " . $usbModel . " - " . $usbSize; ?>
                    </div>
                    <div id="mounted-usb-location_<?php echo $usbName; ?>" class="col-md-2">Mounted
                        at: <?php echo $usbMountLocation; ?></div>
                    <div id="mounted-usb-action_<?php echo $usbName; ?>" class="col-md-2">
                        <input style='width:13em;' type='button' class='buttons btn-danger' value='Force Unmount'
                            onClick='<?php echo $onClick_unmount; ?>'>
                    </div>
                </div>
                <?php if (!empty($openFiles)) {
                    ?>
                    <div id="mounted-usb-open-files" class="row">
                        <div class="backdrop col-md-10 m-auto">
                            <p><b>Open files:</b></p>
                            <pre><?php echo trim($openFiles); ?></pre>
                        </div>
                    </div>
                    <?php
                }
                ?>

            </div>
            <hr>
            <?php
        }
    }
}
//Print a message if no devices were found
if (empty($systems_UsbDevices) || !$foundAMountedDevice) {
    ?>
    <div class="col-md-3">No Mounted USB Detected.</div>
    <?php
}
?>

<div id="mounted-device-callout-warning" class="callout callout-warning">
    Use caution when unmounting USB devices as they may be in use by the system processes. Forcing a unmount may result
    incomplete data or data loss.
</div>