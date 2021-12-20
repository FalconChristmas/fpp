<!DOCTYPE html>
<html>
<?php
require_once 'common.php';
require_once 'config.php';

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . getFPPVersion();

$serialNumber = exec("sed -n 's/^Serial.*: //p' /proc/cpuinfo", $output, $return_val);
if ($return_val != 0) {
    unset($serialNumber);
}

unset($output);

if (!file_exists("/etc/fpp/config_version") && file_exists("/etc/fpp/rfs_version")) {
    exec($SUDO . " $fppDir/scripts/upgrade_config");
}

$lastBoot = exec("uptime -s", $output, $return_val);
if ($return_val != 0) {
    $lastBoot = 'Unknown';
}

unset($output);

$os_build = "Unknown";
if (file_exists("/etc/fpp/rfs_version")) {
    $os_build = exec("cat /etc/fpp/rfs_version", $output, $return_val);
    if ($return_val != 0) {
        $os_build = "Unknown";
    }

    unset($output);
}

$os_version = "Unknown";
if (file_exists("/etc/os-release")) {
    $info = parse_ini_file("/etc/os-release");
    if (isset($info["PRETTY_NAME"])) {
        $os_version = $info["PRETTY_NAME"];
    }

    unset($output);
}

$kernel_version = exec("/bin/uname -r", $output, $return_val);
if ($return_val != 0) {
    $kernel_version = "Unknown";
}

unset($output);

//Get local git version
$git_version = get_local_git_version();

//Get git branch
$git_branch = get_git_branch();

//Remote Git branch version
$git_remote_version = get_remote_git_version($git_branch);

//System uptime
$uptime = get_server_uptime();

function getSymbolByQuantity($bytes)
{
    $symbols = array('B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB');
    $exp = floor(log($bytes) / log(1024));

    return sprintf('%.2f ' . $symbols[$exp], ($bytes / pow(1024, floor($exp))));
}

function getFileCount($dir)
{
    $i = 0;
    if ($handle = opendir($dir)) {
        while (($file = readdir($handle)) !== false) {
            if (!in_array($file, array('.', '..')) && !is_dir($dir . $file)) {
                $i++;
            }

        }
    }

    return $i;
}

?>

<head>
<?php include 'common/menuHead.inc';?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<script language="Javascript">
var osAssetMap={};
$(document).ready(function() {
OnSystemStatusChange(updateSensorStatus)
UpdateVersionInfo();
AppendGithubOS();
GetItemCount('api/configfile/commandPresets.json', 'commandPresetCount', 'commands');
GetItemCount('api/configfile/schedule.json', 'scheduleCount');
$('.default-value').each(function() {
var default_value = this.value;
$(this).focus(function() {
if(this.value == default_value) {
this.value = '';
$(this).css('color', '#333');
}
});
$(this).blur(function() {
if(this.value == '') {
$(this).css('color', '#999');
this.value = default_value;
}
});
});
});

function showHideOsSelect() {
    if ($('#OSSelect option').length > 0) {
        $('#osSelectRow').show();
    } else {
        $('#osSelectRow').hide();
    }
}


function AppendGithubOS() {
    showHideOsSelect();
    $.get("api/git/releases/os", function(data) {
        if ("files" in data) {
            for (const file of data["files"]) {
                if (! file["downloaded"]) {
                    osAssetMap[file["asset_id"]] = {
                        name: file["filename"],
                        url: file["url"]
                    };

                    $('#OSSelect').append($('<option>', {
                        value: file["asset_id"],
                        text: file["filename"] + " (download)"
                    }));
                }
            }
        }
        showHideOsSelect();
    });
}

function CloseUpgradeDialog() {
    $('#upgradePopup').fppDialog('close');
    location.reload();
}

function UpdateVersionInfo() {
    $.get('api/system/status', function(data) {
        $('#fppVersion').html(data.advancedView.Version);
        $('#fppdUptime').html(data.uptime);
        $('#osVersion').html(data.advancedView.OSVersion);
        $('#osRelease').html(data.advancedView.OSRelease);

        var localVer = data.advancedView.LocalGitVersion + " <a href='changelog.php'>ChangeLog</a>";
        var remoteVer = data.advancedView.RemoteGitVersion;
        if ((data.advancedView.RemoteGitVersion != "") &&
            (data.advancedView.RemoteGitVersion != "Unknown") &&
            (data.advancedView.RemoteGitVersion != data.advancedView.LocalGitVersion)) {
            localVer += " <b class='text-success'>(Update is available)</b>";
            remoteVer += " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
        }

        $('#localGitVersion').html(localVer);
        $('#remoteGitVersion').html(remoteVer);
    });
}

function UpgradeDone() {
    UpdateVersionInfo();
    $('#closeDialogButton').show();
}

function UpgradeOS() {
    var os = $('#OSSelect').val();
    var osName = os;
    var extra = "";
    if (os in osAssetMap) {
        osName = osAssetMap[os].name;
        os = osAssetMap[os].url;
    }
    if (confirm('Upgrade the OS using ' + osName + '?\nThis can take a long time. It is also strongly recommended to run FPP backup first.')) {
        $('#closeDialogButton').hide();
        $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "FPP OS Upgrade", dialogClass: 'no-close' });
        $('#upgradePopup').fppDialog( "moveToTop" );
        $('#upgradeText').html('');

        StreamURL('upgradeOS.php?wrapped=1&os=' + os, 'upgradeText', 'UpgradeDone');
    }
}

function UpgradeFPP() {
    $('#closeDialogButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "FPP Upgrade", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('');

    StreamURL('manualUpdate.php?wrapped=1', 'upgradeText', 'UpgradeDone');
}
function UpgradeFirmwareDone() {
    var txt = $('#upgradeText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to replace the firmware for cape:\n" + arrayOfLines[1] + "\n\nWith the firmware for: \n" + arrayOfLines[2] + "\n";
        if (confirm(msg)) {
            let formData = new FormData();
            let firmware = document.getElementById("firmware").files[0];
            formData.append("firmware", firmware);

            $('#upgradeText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true', 'upgradeText', 'UpgradeDone', 'UpgradeDone', 'POST', formData, false, false);
        }
    }
    $('#closeDialogButton').show();
}
function UpgradeFirmware() {
    let firmware = document.getElementById("firmware").files[0];
    if (firmware == "" || firmware == null) {
        return;
    }
    let formData = new FormData();
    formData.append("firmware", firmware);

    $('#closeDialogButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Upgrade Cape Firmware", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('');
    StreamURL('upgradeCapeFirmware.php', 'upgradeText', 'UpgradeFirmwareDone', 'UpgradeFirmwareDone', 'POST', formData, false, false);
}

</script>
<title><?echo $pageTitle; ?></title>
<style>
.no-close .ui-dialog-titlebar-close {display: none }

.clear {
  clear: both;
}
.items {
  width: 40%;
  background: rgb#FFF;
  float: right;
  margin: 0, auto;
}
.selectedEntry {
  background: #CCC;
}
.pl_title {
  font-size: larger;
}

h4, h3 {
  padding: 0;
  margin: 0;
}
.tblheader {
  background-color: #CCC;
  text-align: center;
}
tr.rowScheduleDetails {
  border: thin solid;
  border-color: #CCC;
}
tr.rowScheduleDetails td {
  padding: 1px 5px;
}
#tblSchedule {
  border: thin;
  border-color: #333;
  border-collapse: collapse;
}
.time {
  width: 100%;
}
.center {
  text-align: center;
}
</style>
</head>

<body>
<div id="bodyWrapper">
  <?php
$activeParentMenuItem = 'help';
include 'menu.inc';?>
  <div class="mainContainer">
    <div class="title">About FPP</div>
    <div class="pageContent">

      <div>
          <div style="overflow: hidden; padding: 10px;">
          <div class="row">
            <div class="aboutLeft col-md">
              <table class='tblAbout'>
                <tr><td><b>Version Info</b></td><td>&nbsp;</td></tr>
                <tr><td>FPP Version:</td><td id='fppVersion'><?echo $fpp_version; ?></td></tr>
                <tr><td>Platform:</td><td><?
echo $settings['Platform'];
if (($settings['Variant'] != '') && ($settings['Variant'] != $settings['Platform'])) {
    echo " (" . $settings['Variant'] . ")";
}

?></td></tr>
                <tr><td>FPP OS Build:</td><td id='osVersion'><?echo $os_build; ?></td></tr>
                <tr><td>OS Version:</td><td id='osRelease'><?echo $os_version; ?></td></tr>
    <?if (isset($serialNumber) && $serialNumber != "") {?>
            <tr><td>Hardware Serial Number:</td><td><?echo $serialNumber; ?></td></tr>
    <?}?>
                <tr><td>Kernel Version:</td><td><?echo $kernel_version; ?></td></tr>
                <tr><td>System Boot Time:</td><td id='lastBoot'><?echo $lastBoot; ?></td></tr>
                <tr><td>fppd Uptime:</td><td id='fppdUptime'></td></tr>
                <tr><td>Local Git Version:</td><td id='localGitVersion'>
    <?
echo $git_version;
echo " <a href='changelog.php'>ChangeLog</a>";
if (($git_remote_version != "") &&
    ($git_remote_version != "Unknown") &&
    ($git_version != $git_remote_version)) {
    echo " <b class='text-success'>(Update is available)</b>";
}

?>
                    </td></tr>
                <tr><td>Remote Git Version:</td><td id='remoteGitVersion'>
    <?
echo $git_remote_version;
if (($git_remote_version != "") &&
    ($git_remote_version != "Unknown") &&
    ($git_version != $git_remote_version)) {
    echo " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
}

?>
                    </td></tr>
                <tr><td>Upgrade FPP:</td><td><input type='button' value='Upgrade FPP' onClick='UpgradeFPP();' class='buttons btn-outline-success' id='ManualUpdate'></td></tr>
    <?
if ($settings['uiLevel'] > 0) {
    $upgradeSources = array();
    $remotes = getKnownFPPSystems();

    $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));

    foreach ($remotes as $desc => $host) {
        if ((!in_array($host, $IPs)) && (!preg_match('/^169\.254\./', $host))) {
            $upgradeSources[$desc] = $host;
        }
    }
    $upgradeSources = array("github.com" => "github.com") + $upgradeSources;
    ?>
                <tr><td>FPP Upgrade Source:</td><td><?PrintSettingSelect("Upgrade Source", "UpgradeSource", 0, 0, "github.com", $upgradeSources);?></td></tr>
    <?
}

$osUpdateFiles = preg_grep("/^" . $settings['OSImagePrefix'] . "-/", getFileList($uploadDirectory, "fppos"));
echo "<tr id='osSelectRow'><td>Upgrade OS:</td><td><select class='OSSelect' id='OSSelect'>\n";
foreach ($osUpdateFiles as $key => $value) {
    echo "<option value='" . $value . "'>" . $value . "</option>\n";
}
echo "</select>&nbsp;<input type='button' value='Upgrade OS' onClick='UpgradeOS();' class='buttons' id='OSUpgrade'></td></tr>";
?>
                <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
                <tr><td><b>System Utilization</b></td><td>&nbsp;</td></tr>
                <tr><td>CPU Usage:</td><td><?printf("%.2f", get_server_cpu_usage());?>%</td></tr>
                <tr><td>Memory Usage:</td><td><?printf("%.2f", get_server_memory_usage());?>%</td></tr>
                <tr><td style="vertical-align:top;">Sensors:</td><td><div id="sensorData"></div></td></tr>

                <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
                <tr><td><b>Uptime</b></td><td>&nbsp;</td></tr>
                <tr><td colspan='2'><?echo $uptime; ?></td></tr>

              </table>
              <br/>
            </div>
            <div class="aboutRight col-md">
              <table class='tblAbout'>
                <tr><td><b>Player Stats</b></td><td>&nbsp;</td></tr>
                <tr><td>Schedules:</td><td><a href='scheduler.php' class='nonULLink' id='scheduleCount'></a></td></tr>
                <tr><td>Playlists:</td><td><a href='playlists.php' class='nonULLink'><?echo getFileCount($playlistDirectory); ?></a></td></tr>
                <tr><td>Sequences:</td><td><a href='uploadfile.php' class='nonULLink'><?echo getFileCount($sequenceDirectory); ?></a></td></tr>
                <tr><td>Audio Files:</td><td><a href='uploadfile.php#tab-audio' class='nonULLink'><?echo getFileCount($musicDirectory); ?></a></td></tr>
                <tr><td>Videos:</td><td><a href='uploadfile.php#tab-video' class='nonULLink'><?echo getFileCount($videoDirectory); ?></a></td></tr>
                <tr><td>Command Presets:</td><td><a href='commandPresets.php' class='nonULLink' id='commandPresetCount'></a></td></tr>
                <tr><td>Effects:</td><td><a href='uploadfile.php#tab-effects' class='nonULLink'><?echo getFileCount($effectDirectory); ?></a></td></tr>
                <tr><td>Scripts:</td><td><a href='uploadfile.php#tab-scripts' class='nonULLink'><?echo getFileCount($scriptDirectory); ?></a></td></tr>

                <tr><td>&nbsp;</td><td>&nbsp;</td></tr>

                <tr><td><b>Disk Utilization</b></td><td>&nbsp;</td></tr>
    <?
$diskTotal = disk_total_space("/");
$diskFree = disk_free_space("/");
$percentageUsed = 100 - ($diskFree * 100 / $diskTotal);
$progressClass = "bg-success";
if ($percentageUsed > 60) {$progressClass = "bg-warning";}
if ($percentageUsed > 80) {$progressClass = "bg-danger";}
exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
$rootDevice = $output[0];
unset($output);
exec('findmnt -n -o SOURCE ' . $mediaDirectory . ' | colrm 1 5', $output, $return_val);
$mediaDevice = "";
if (count($output) > 0) {
    $mediaDevice = $output[0];
}
unset($output);
?>
                <tr>
                  <td colspan="2">
                  <div class="progress">
                    <div class="progress-bar <?echo $progressClass; ?>" role="progressbar" style="width: <?printf("%2.0f%%", $percentageUsed);?>;" aria-valuenow="25" aria-valuemin="0" aria-valuemax="100"><?printf("%2.0f%%", $percentageUsed);?></div>
                  </div>
                  </td>
                </tr>
                <tr><td>Root (<?echo $rootDevice; ?>) Free Space:</td><td>

    <?
printf("%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
?>
                  </td></tr>
    <?
if (isset($mediaDevice) && $mediaDevice != "" && $mediaDevice != $rootDevice) {
    $diskTotal = disk_total_space($mediaDirectory);
    $diskFree = disk_free_space($mediaDirectory);
    $percentageUsed = 100 - ($diskFree * 100 / $diskTotal);
    $progressClass = "bg-success";
    if ($percentageUsed > 60) {$progressClass = "bg-warning";}
    if ($percentageUsed > 80) {$progressClass = "bg-danger";}
    ?>
                  <tr>
                  <td colspan="2">
                  <div class="progress mt-2">
                    <div class="progress-bar <?echo $progressClass; ?>" role="progressbar" style="width:<?printf("%2.0f%%", $percentageUsed);?>;" aria-valuenow="25" aria-valuemin="0" aria-valuemax="100"><?printf("%2.0f%%", $percentageUsed);?></div>
                  </div>
                  </td>
                </tr>
                <tr><td>Media (<?echo $mediaDevice; ?>) Free Space:</td><td>
    <?
    printf("%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
    ?>
                  </td></tr>
                <tr><td></td><td></td></tr>
    <?}?>
              </table>
            </div>
        <div class="clear"></div>
        </fieldset>
        <?
if (isset($settings["cape-info"])) {
    $currentCapeInfo = $settings["cape-info"];
    ?>
        </div>
        <div class="row">
            <div class="aboutAll col-md">
            <h2>About Cape/Hat</h2>
            <div class="container-fluid">
            <div class="row">
            <div class='<?if (isset($currentCapeInfo['vendor'])) {echo "aboutLeft col-md";} else {echo "aboutAll";}?> '>
            <table class='tblAbout'>
            <tr><td><b>Name:</b></td><td width="100%"><?echo $currentCapeInfo['name'] ?></td></tr>
            <?
    if (isset($currentCapeInfo['version'])) {
        echo "<tr><td><b>Version:</b></td><td>" . $currentCapeInfo['version'] . "</td></tr>";
    }
    if (isset($currentCapeInfo['serialNumber'])) {
        echo "<tr><td><b>Serial&nbsp;Number:</b></td><td>" . $currentCapeInfo['serialNumber'] . "</td></tr>";
    }
    if (isset($currentCapeInfo['designer'])) {
        echo "<tr><td><b>Designer:</b></td><td>" . $currentCapeInfo['designer'] . "</td></tr>";
    }
    if (isset($currentCapeInfo['description'])) {
        echo "<tr><td colspan=\"2\">";
        if (isset($currentCapeInfo['vendor']) || $currentCapeInfo['name'] == "Unknown") {
            echo $currentCapeInfo['description'];
        } else {
            echo htmlspecialchars($currentCapeInfo['description']);
        }
        echo "</td></tr>";
    }
    ?>
            </table>
            </div>
            <?if (isset($currentCapeInfo['vendor'])) {?>
                   <div class='aboutRight col-md'>
                   <table class='tblAbout'>
                        <tr><td><b>Vendor&nbsp;Name:</b></td><td><?echo $currentCapeInfo['vendor']['name'] ?></td></tr>
                <?if (isset($currentCapeInfo['vendor']['url'])) {
        $url = $currentCapeInfo['vendor']['url'];
        $landing = $url;
        if (isset($currentCapeInfo['vendor']['landingPage'])) {
            $landing = $currentCapeInfo['vendor']['landingPage'];
        }
        if ($settings['SendVendorSerial'] == 1) {
            $landing = $landing . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
        }
        if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "" && $settings['SendVendorSerial'] == 1) {
            $landing = $landing . "&cs=" . $currentCapeInfo['cs'];
        }
        echo "<tr><td><b>Vendor&nbsp;URL:</b></td><td><a href=\"" . $landing . "\">" . $url . "</a></td></tr>";
    }
        if (isset($currentCapeInfo['vendor']['phone'])) {
            echo "<tr><td><b>Phone&nbsp;Number:</b></td><td>" . $currentCapeInfo['vendor']['phone'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['email'])) {
            echo "<tr><td><b>E-mail:</b></td><td><a href=\"mailto:" . $currentCapeInfo['vendor']['email'] . "\">" . $currentCapeInfo['vendor']['email'] . "</td></tr>";
        }

        if (isset($currentCapeInfo['vendor']['image']) && $settings['FetchVendorLogos']) {
            if ($settings['SendVendorSerial'] == 1) {
                $iurl = $currentCapeInfo['vendor']['image'] . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
            }
            if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "" && $settings['SendVendorSerial'] == 1) {
                $iurl = $iurl . "&cs=" . $currentCapeInfo['cs'];
            }
            echo "<tr><td colspan=\"2\"><a href=\"" . $landing . "\"><img style='max-height: 90px; max-width: 300px;' src=\"" . $iurl . "\" /></a></td></tr>";
        }?>
                   </table>
                   </div>
                <?}?>
            </div> <!-- row -->
            </div> <!-- container -->
            </div> <!-- col -->
        </div> <!-- row -->



        <?
}
$eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
if ($settings['Platform'] == "BeagleBone Black") {
    $eepromFile = "/sys/bus/i2c/devices/2-0050/eeprom";
    if (!file_exists($eepromFile)) {
        $eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
    }
}
if (file_exists($eepromFile)) {
    ?>
        <div class="row">
            <div class="aboutAll col-md">
            <br><h2>Cape/Hat Firmware Upgrade</h2>
            <div class="container-fluid"><div class="row"><div class="aboutAll col-md">
            <label for="firmware">Firmware file:</label><input type="file" name="firmware" id="firmware"/>&nbsp;<input type='button' class="buttons" value='Upgrade' onClick='UpgradeFirmware();'  id='UpdateFirmware'>
            </div>
            </div>
            </div>

            </div>
        </div>

            <?
}
?>

        <div id='logViewer' title='Log Viewer' style="display: none">
          <pre>
            <div id='logText'>
            </div>
          </pre>
        </div>
        </div>
      </div>
    </div>
  </div>
  <?php include 'common/footer.inc';?>
</div>
<div id='upgradePopup' title='FPP Upgrade' style="display: none">
    <textarea style='width: 99%; height: 500px;' disabled id='upgradeText'>
    </textarea>
    <input id='closeDialogButton' type='button' class='buttons' value='Close' onClick='CloseUpgradeDialog();' style='display: none;'>
</div>
</body>
</html>
