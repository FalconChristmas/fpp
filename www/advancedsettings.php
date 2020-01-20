<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>

<script type="text/javascript" src="jquery/jQuery.msgBox/scripts/jquery.msgBox.js"></script>
<link href="jquery/jQuery.msgBox/styles/msgBoxLight.css" rel="stylesheet" type="text/css">

<script>
function growSDCardFS() {
    $('#dialog-confirm')
        .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
                $(this).dialog("close");
                window.location.href="growsd.php";
                SetRebootFlag();
            },
            "No" : function() {
                $(this).dialog("close");
             }
        }
    });
}
function newSDCardPartition() {
    $('#dialog-confirm-newpartition')
    .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
            $(this).dialog("close");
            window.location.href="newpartitionsd.php";
            SetRebootFlag();
            },
            "No" : function() {
            $(this).dialog("close");
            }
            }
            });
}

<?php
    if ($settings['Platform'] == "BeagleBone Black") {
?>

function flashEMMC() {
    $('#dialog-confirm-emmc')
        .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
                "Yes" : function() {
                $(this).dialog("close");
                window.location.href="flashbbbemmc.php";
            },
            "No" : function() {
                $(this).dialog("close");
            }
        }
    });
}
function flashEMMCBtrfs() {
    $('#dialog-confirm-emmc')
    .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
            $(this).dialog("close");
            window.location.href="flashbbbemmc-btrfs.php";
            },
            "No" : function() {
            $(this).dialog("close");
            }
            }
            });
}
<?php
    }
    
    
function PrintStorageDeviceSelect($platform)
{
	global $SUDO;

	# FIXME, this would be much simpler by parsing "lsblk -l"
	exec('lsblk -l | grep /boot | cut -f1 -d" " | sed -e "s/p[0-9]$//"', $output, $return_val);
    if (count($output) > 0) {
        $bootDevice = $output[0];
    } else {
        $bootDevice = "";
    }
	unset($output);

    if ($platform == "BeagleBone Black") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
        $rootDevice = $output[0];
        unset($output);
        
        if ($bootDevice == "") {
            exec('findmnt -n -o SOURCE / | colrm 1 5 | sed -e "s/p[0-9]$//"', $output, $return_val);
            $bootDevice = $output[0];
            unset($output);
        }
    } else {
        exec('lsblk -l | grep " /$" | cut -f1 -d" "', $output, $return_val);
        $rootDevice = $output[0];
        unset($output);
    }

	$storageDevice = "";
	exec('grep "fpp/media" /etc/fstab | cut -f1 -d" " | sed -e "s/\/dev\///"', $output, $return_val);
	if (isset($output[0]))
		$storageDevice = $output[0];
	unset($output);

	$found = 0;
	$values = Array();

	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^sd[a-z][0-9]/", $fileName)) ||
			(preg_match("/^mmcblk[0-9]p[0-9]/", $fileName)))
		{
			exec($SUDO . " sfdisk -s /dev/$fileName", $output, $return_val);
			$GB = intval($output[0]) / 1024.0 / 1024.0;
			unset($output);

			if ($GB <= 0.1)
				continue;

			$FreeGB = "Not Mounted";
			exec("df -k /dev/$fileName | grep $fileName | awk '{print $4}'", $output, $return_val);
			if (count($output))
			{
				$FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
				unset($output);
			}
			else
			{
				unset($output);

				if (preg_match("/^$rootDevice/", $fileName))
				{
					exec("df -k / | grep ' /$' | awk '{print \$4}'", $output, $return_val);
					if (count($output))
						$FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
					unset($output);
				}
			}

			$key = $fileName . " ";
			$type = "";

			if (preg_match("/^$bootDevice/", $fileName))
			{
				$type .= " (boot device)";
			}

			if (preg_match("/^sd/", $fileName))
			{
				$type .= " (USB)";
			}

			$key = sprintf( "%s - %.1fGB (%s) %s", $fileName, $GB, $FreeGB, $type);

			$values[$key] = $fileName;

			if ($storageDevice == $fileName)
				$found = 1;
		}
	}

	if (!$found)
	{
		$arr = array_reverse($values, true);
		$values = array_reverse($arr);
	}
    if ($storageDevice == "") {
        $storageDevice = $rootDevice;
    }

	PrintSettingSelect('StorageDevice', 'storageDevice', 0, 1, $storageDevice, $values, "", "", "checkFormatStorage");
}

?>


function checkForStorageCopy() {
    $.msgBox({
             title: "Copy settings?",
             content: "Would you like to copy all files to the new storage location?\nAll settings on the new storage will be overwritten.",
             type: "info",
             buttons: [{ value: "Yes" }, { value: "No" }],
             success: function (result) {
                 storageDeviceChanged();
                 if (result == "Yes") {
                    window.location.href="copystorage.php?storageLocation=" + $('#storageDevice').val() + "&direction=TO&path=/&flags=All";
                 }
            }
        });
}

function checkFormatStorage()
{
    var value = $('#storageDevice').val();
    
    var e = document.getElementById("storageDevice");
    var name = e.options[e.selectedIndex].text;
    if (name.includes("Not Mounted")) {
        var btitle = "Format Storage Location (" + value + ")" + name;
        $.msgBox({ type: "prompt",
                 title: btitle,
                 inputs: [
                     { header: "Don't Format", type: "radio", name: "formatType", checked:"", value: "none" },
                     { header: "ext4 (Most stable)", type: "radio", name: "formatType", value: "ext4" },
                     { header: "FAT (Compatible with Windows/OSX, unsupported, slow)", type: "radio", name: "formatType", value: "FAT"}
                 ],
                 buttons: [ { value: "OK" } ],
                 opacity: 0.5,
                 success: function (result, values) {
                 var v = $('input[name=formatType]:checked').val();
                 if (v != "none") {
                    $.ajax({ url: "formatstorage.php?fs=" + v + "&storageLocation=" + $('#storageDevice').val(),
                        async: false,
                        success: function(data) {
                           checkForStorageCopy();
                        },
                        failure: function(data) {
                        DialogError("Formate Storage", "Error formatting storage.");
                        }
                        });
                    } else {
                        checkForStorageCopy();
                    }
                 }
                 });
    } else {
        storageDeviceChanged();
    }
}


</script>

</head>
<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="global" class="settings">
	<fieldset>
	<legend>FPP Advanced Settings</legend>
	<table table width = "100%">
        <? if ($settings['SubPlatform'] != "Docker" ) { ?>
        <tr>
          <td><b>Storage Device:</b></td>
          <td><? PrintStorageDeviceSelect($settings['Platform']); ?><br>
            Changing the storage device to anything other than the SD card is strongly discouraged.   There are all kinds of problems that using USB storage introduce into the system which can easily result in various problems include network lag, packet drops, audio clicks/pops, high CPU usage, etc...  Using USB storage also results in longer bootup time.   In addition, many advanced features and various capes/hats are known to NOT work when using USB storage.
            <p>
            In addition to the above, since it is not recommended, using USB storage is not tested nearly as extensively by the FPP developers.   Thus, upgrades (even "patch" upgrades) have a higher risk of unexpected problems.   By selecting a USB storage device, you assume much higher risk of problems and issues than when selecting an SD partition.
            </td>
        </tr>
        <tr><td colspan='2'><hr></td></tr>
    <? } ?>
<?
    $addnewfsbutton = false;
    $addflashbutton = false;
    exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
    $rootDevice = $output[0];
    if ($rootDevice == 'mmcblk0p1' || $rootDevice == 'mmcblk0p2') {
        if (isset($settings["LastBlock"]) && $settings['LastBlock'] < 7200000) {
            $addnewfsbutton = true;
        }
        if ($settings['Platform'] == "BeagleBone Black") {
            if (strpos($settings['SubPlatform'], 'PocketBeagle') === FALSE) {
                $addflashbutton = true;
            }
        }
    }
    if ($addnewfsbutton) {
?>
        <tr><td colspan='2'><hr></td></tr>
        <tr><td>
        <input type='button' class='buttons' value='Grow Filesystem' onClick='growSDCardFS();'>
        </td>
        <td><b>Grow file system on SD card</b> - This will grow the file system on the SD card to use
        the entire size of the SD card.</td>
        </tr>
        <tr><td colspan='2'><hr></td></tr>
        <tr><td>
        <input type='button' class='buttons' value='New Partition' onClick='newSDCardPartition();'>
        </td>
        <td><b>New partition on SD card</b> - This will create a new partition in the unused aread of the SD card.  The new partition can be selected as a storage location and formatted to BTRFS or ext4 after a reboot.</td>
        </tr>
<?php
	}
    if ($addflashbutton) {
?>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
                <input type='button' class='buttons' value='Flash to eMMC' onClick='flashEMMC();'>
                </td>
                <td><b>Flash to eMMC</b> - This will copy FPP to the internal eMMC.</td>
            </tr>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
                <input type='button' class='buttons' value='Flash to eMMC' onClick='flashEMMCBtrfs();'>
                </td>
<td><b>Flash to eMMC - BTRFS root</b> - This will copy FPP to the internal eMMC, but use BTRFS for the root filesystem.  BTRFS uses compression to save a lot of space on the eMMC, but at the expense of extra CPU usage.</td>
            </tr>
<?php
    }
?>
	</table>
	</fieldset>
</div>
<div id="dialog-confirm" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Growing the filesystem will sometimes require a reboot to take effect.  Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-emmc" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Flashing the eMMC can take a long time.  Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-newpartition" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Creating a new partition in the unused space will require a reboot to take effect.  Do you wish to proceed?</p>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
