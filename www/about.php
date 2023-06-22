<!DOCTYPE html>
<html>
<?php
require_once 'common.php';
require_once 'config.php';

writeFPPVersionJavascriptFunctions();

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = getFPPVersion();

if (file_exists("/proc/cpuinfo")) {
    $serialNumber = exec("sed -n 's/^Serial.*: //p' /proc/cpuinfo", $output, $return_val);
    if ($return_val != 0) {
        unset($serialNumber);
    }
}

unset($output);

if (!file_exists("/etc/fpp/config_version") && file_exists("/etc/fpp/rfs_version")) {
    exec($SUDO . " $fppDir/scripts/upgrade_config");
}

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
if ($settings["Platform"] != "MacOS") {
    $lastBoot = exec("uptime -s", $output, $return_val);
    if ($return_val != 0) {
        $lastBoot = 'Unknown';
    }
    unset($output);
} else {
    $lastBoot = "";
    $os_version = "";
    $os_build = "";
}

$kernel_version = exec("uname -r", $output, $return_val);
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

$uploadDirectory = $mediaDirectory . "/upload";
$freeSpace = disk_free_space($uploadDirectory);
?>

<head>
<?php include 'common/menuHead.inc';?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<script language="Javascript">
var osAssetMap={};
var originalFPPOS;
$(document).ready(function() {
OnSystemStatusChange(updateSensorStatus)
UpdateVersionInfo();
originalFPPOS = $('#OSSelect').html();
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
    if ($('#OSSelect option').length > 1) {
        $('#osSelectRow').show();
    } else {
        $('#osSelectRow').hide();
    }
}


function AppendGithubOS() {
    showHideOsSelect();
<?
// we want at least a GB in order to be able to download the fppos and have space to then apply it
if ($freeSpace > 1000000000) {
?>
    var allPlatforms = '';

    if ($('#allPlatforms').is(':checked')) {
        allPlatforms = 'api/git/releases/os/all';
    } else {
        allPlatforms = 'api/git/releases/os';
    }

    $.get(allPlatforms, function(data) {
        var devMode = (settings['uiLevel'] && (parseInt(settings['uiLevel']) == 3));
        if ("files" in data) {
            for (const file of data["files"]) {
                if (!file["downloaded"] && (devMode || !file['filename'].match(/-v?(4\.|5\.[0-4])/))) {
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
<? } ?>
}

function CloseFPPUpgradeDialog() {
    CloseModalDialog("upgradePopupStatus");
    location.reload();
}
function FPPUpgradeDone() {
    if (statusTimeout === null)
        LoadSystemStatus();

    UpdateVersionInfo();
    $("#fppUpgradeCloseDialogButton").prop("disabled", false);
    EnableModalDialogCloseButton("upgradePopupStatus");
}
function DownloadDone() {
    $("#fppDownloadCloseDialogButton").prop("disabled", false);
    EnableModalDialogCloseButton("downloadPopupStatus");
}
function UpgradeDone() {
    if (statusTimeout === null)
        LoadSystemStatus();

    UpdateVersionInfo();
    $("#fppUpgradeOSCloseDialogButton").prop("disabled", false);
    EnableModalDialogCloseButton("upgradeOSPopupStatus");
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

function UpgradeOS() {
    var os = $('#OSSelect').val();
    var osName = os;
    var extra = "";

    if (os == '')
        return;

    var keepOptFPP = '';
    if ($('#keepOptFPP').is(':checked')) {
        keepOptFPP = '&keepOptFPP=1';
    }

    if (os in osAssetMap) {
        osName = osAssetMap[os].name;
        os = osAssetMap[os].url;
    }
    if (confirm('Upgrade the OS using ' + osName + '?\nThis can take a long time. It is also strongly recommended to run FPP backup first.')) {

        var options = {
            id: "upgradeOSPopupStatus",
            title: "FPP OS Upgrade",
            body: "<textarea style='width: 99%; height: 500px;' disabled id='streamedUpgradeOSText'></textarea>",
            class: "modal-dialog-scrollable",
            noClose: true,
            keyboard: false,
            backdrop: "static",
            footer: "",
            buttons: {
                "Close": {
                    id: 'fppUpgradeOSCloseDialogButton',
                    click: function() {CloseModalDialog("upgradeOSPopupStatus");location.reload();},
                    disabled: true,
                    class: 'btn-success'
                }
            }
        };
        $("#fppUpgradeOSCloseDialogButton").prop("disabled", true);
        DoModalDialog(options);

        clearTimeout(statusTimeout);
        statusTimeout = null;

        StreamURL('upgradeOS.php?wrapped=1&os=' + os + keepOptFPP, 'streamedUpgradeOSText', 'UpgradeDone');
    }
}

function DownloadOS() {
    var os = $('#OSSelect').val();
    var osName = os;
    var extra = "";

    if (os == '')
        return;

    if (os in osAssetMap) {
        osName = osAssetMap[os].name;
        os = osAssetMap[os].url;

        var options = {
            id: "downloadPopupStatus",
            title: "FPP Download OS Image",
            body: "<textarea style='width: 99%; height: 500px;' disabled id='streamedUDownloadText'></textarea>",
            class: "modal-dialog-scrollable",
            noClose: true,
            keyboard: false,
            backdrop: "static",
            footer: "",
            buttons: {
                "Close": {
                    id: 'fppDownloadCloseDialogButton',
                    click: function() {CloseModalDialog("downloadPopupStatus");location.reload();},
                    disabled: true,
                    class: 'btn-success'
                }
            }
        };
        $("#fppDownloadCloseDialogButton").prop("disabled", true);
        DoModalDialog(options);

        StreamURL('upgradeOS.php?wrapped=1&downloadOnly=1&os=' + os, 'streamedUDownloadText', 'DownloadDone');
    } else {
        alert('This fppos image has already been downloaded.');
    }
}

function OSSelectChanged() {
    var os = $('#OSSelect').val();
<?
// we want at least a 200MB in order to be able to apply the fppos
if ($freeSpace < 200000000) {
    echo  "os = '';\n";
}
?>
    if (os == '') {
        $('#OSUpgrade').attr('disabled', 'disabled');
        $('#OSDownload').attr('disabled', 'disabled');
    } else {
        $('#OSUpgrade').removeAttr('disabled');
        if (os in osAssetMap) {
            $('#OSDownload').removeAttr('disabled');
        } else {
            $('#OSDownload').attr('disabled', 'disabled');
        }
    }
}

function UpgradeFPP() {
    clearTimeout(statusTimeout);
    statusTimeout = null;
    
    var options = {
        id: "upgradePopupStatus",
        title: "FPP Upgrade",
        body: "<textarea style='width: 99%; height: 500px;' disabled id='streamedUpgradeText'></textarea>",
        class: "modal-dialog-scrollable",
        noClose: true,
        keyboard: false,
        backdrop: "static",
        footer: "",
        buttons: {
            "Close": {
                id: 'fppUpgradeCloseDialogButton',
                click: function() {CloseFPPUpgradeDialog();},
                disabled: true,
                class: 'btn-success'
            }
        }
    };
    $("#fppUpgradeCloseDialogButton").prop("disabled", true);
    DoModalDialog(options);
    StreamURL('manualUpdate.php?wrapped=1', 'streamedUpgradeText', 'FPPUpgradeDone');
}

function UpdatePlatforms() {  
    var allPlatforms = '';
    $('#OSSelect').html(originalFPPOS);
    AppendGithubOS();
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
     <?if ($os_build != "") {?><tr><td>FPP OS Build:</td><td id='osVersion'><?echo $os_build; ?></td></tr><?}?>
                <tr><td>OS Version:</td><td id='osRelease'><?echo $os_version; ?></td></tr>
    <?if (isset($serialNumber) && $serialNumber != "") {?>
            <tr><td>Hardware Serial Number:</td><td><?echo $serialNumber; ?></td></tr>
    <?}?>
                <tr><td>Kernel Version:</td><td><?echo $kernel_version; ?></td></tr>
<?if ($lastBoot != "") {?>
                <tr><td>System Boot Time:</td><td id='lastBoot'><?echo $lastBoot; ?></td></tr>
<?}?>
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

    if ($settings["Platform"] != "MacOS") {
        $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));
    } else {
        $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | grep 'inet ' | awk '{print \$2}'")));
    }
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

$osUpdateFiles = getFileList($uploadDirectory, "fppos");
echo "<tr id='osSelectRow'><td style='vertical-align: top;'>Upgrade OS:</td><td><select class='OSSelect' id='OSSelect' onChange='OSSelectChanged();'>\n";
echo "<option value=''>-- Choose an OS Version --</option>\n";
foreach ($osUpdateFiles as $key => $value) {
    echo "<option value='" . $value . "'>" . $value . "</option>\n";
}
echo "</select><span";
if (getFPPBranch() != 'master') {
    echo " style='display: none;'";
}
echo ">&nbsp&nbsp;";
if ($settings['uiLevel'] > 0) {
    echo "<br>Show All Platforms <input type='checkbox' id='allPlatforms' onClick='UpdatePlatforms();'><img id='allPlatforms_img' title='Show both BBB & Pi downloads' src='images/redesign/help-icon.svg' class='icon-help'></span>";
}
echo "&nbsp;&nbsp;<br>Preserve /opt/fpp <input type='checkbox' id='keepOptFPP'><img id='keepOptFPP_img' title='Preserve the FPP version in /opt/fpp across fppos upgrade.' src='images/redesign/help-icon.svg' class='icon-help'></span>";


echo "<br><input type='button' disabled value='Upgrade OS' onClick='UpgradeOS();' class='buttons' id='OSUpgrade'>&nbsp;<input type='button' disabled value='Download Only' onClick='DownloadOS();' class='buttons' id='OSDownload'></td></tr>";
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
if (file_exists("/bin/findmnt")) {
    exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
    $rootDevice = $output[0];
    unset($output);
    exec('findmnt -n -o SOURCE ' . $mediaDirectory . ' | colrm 1 5', $output, $return_val);
    $mediaDevice = "";
    if (count($output) > 0) {
        $mediaDevice = $output[0];
    }
    unset($output);
} else {
    $rootDevice = "";
    $mediaDevice = "";
}
?>
                <tr>
                  <td colspan="2">
                  <div class="progress">
                    <div class="progress-bar <?echo $progressClass; ?>" role="progressbar" style="width: <?printf("%2.0f%%", $percentageUsed);?>;" aria-valuenow="25" aria-valuemin="0" aria-valuemax="100"><?printf("%2.0f%%", $percentageUsed);?></div>
                  </div>
                  </td>
                </tr>
                <tr><td>Root <?if ($rootDevice != "") {
    echo "(" . $rootDevice . ")";
}
?> Free Space:</td><td>

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

<?
if (!isset($settings['cape-info']) || !isset($settings['cape-info']['verifiedKeyId']) || ($settings['cape-info']['verifiedKeyId'] != 'fp')) {
?>
                <tr><td style='height: 50px;'></td></tr>
                <tr><td colspan='2' style='width: 300px;'>
                    If you would like to donate to the Falcon Player development team to help support the continued development of FPP, you can use the donate button below.<br>
                    <form action="https://www.paypal.com/donate" method="post" target="_top"><input type="hidden" name="hosted_button_id" value="ASF9XYZ2V2F5G" /><input style="height: 60px;" type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" border="0" name="submit" title="Donate to the Falcon Player" alt="Donate to the Falcon Player" /><img alt="" border="0" src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1" height="1" /></form>
                    </td></tr>
<?
}
?>
              </table>
            </div>
        <div class="clear"></div>
        </fieldset>
    </div>
<?
$eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
if ($settings['Platform'] == "BeagleBone Black") {
    $eepromFile = "/sys/bus/i2c/devices/2-0050/eeprom";
    if (!file_exists($eepromFile)) {
        $eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
    }
}
if (!file_exists($eepromFile) && file_exists("/home/fpp/media/config/cape-eeprom.bin")) {
    $eepromFile = "/home/fpp/media/config/cape-eeprom.bin";
}
if (file_exists($eepromFile)) {
    echo "<hr>For Cape information and EEPROM signing, go to the <a href='cape-info.php'>Cape Info</a> page.\n";
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
</body>
</html>
