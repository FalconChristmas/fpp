<!DOCTYPE html>
<html lang="en">
<?php
include 'common/htmlMeta.inc';
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
$git_remote_version = get_remote_git_version();

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
        closedir($handle);
    }

    return $i;
}

$uploadDirectory = $mediaDirectory . "/upload";
$freeSpace = disk_free_space($uploadDirectory);
?>

<head>
    <? include 'common/menuHead.inc'; ?>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <script language="Javascript">
        var osAssetMap = {};

        //Page Load Logic
        function pageSpecific_PageLoad_DOM_Setup() {
            UpdateVersionInfo();
            PopulateOSSelect();
            GetItemCount('api/configfile/commandPresets.json', 'commandPresetCount', 'commands');
            GetItemCount('api/configfile/schedule.json', 'scheduleCount');
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            OnSystemStatusChange(updateSensorStatus);
            $('.default-value').each(function () {
                var default_value = this.value;
                $(this).on("focus", function () {
                    if (this.value == default_value) {
                        this.value = '';
                        $(this).css('color', '#333');
                    }
                });
                $(this).blur(function () {
                    if (this.value == '') {
                        $(this).css('color', '#999');
                        this.value = default_value;
                    }
                });
            });
        }

        function showHideOsSelect() {
            if ($('#OSSelect option').length > 1) {
                $('#osSelectRow').show();
            } else {
                $('#osSelectRow').hide();
            }
        }


        function PopulateOSSelect() {
            showHideOsSelect();
            <?
            //get listing of files already downloaded in case no git connection available
            $osUpdateFiles = getFileList($uploadDirectory, "fppos");

            // we want at least a GB in order to be able to download the fppos and have space to then apply it
            if ($freeSpace > 1000000000) {
                ?>

                var allPlatforms = '';
                if ($('#allPlatforms').is(':checked')) {
                    allPlatforms = 'api/git/releases/os/all';
                } else {
                    allPlatforms = 'api/git/releases/os';
                }

                //cleanup previous load values
                $('#OSSelect option').filter(function () { return parseInt(this.value) > 0; }).remove();

                $.get(allPlatforms, function (data) {
                    var devMode = (settings['uiLevel'] && (parseInt(settings['uiLevel']) == 3));
                    if ("files" in data) {
                        for (const file of data["files"]) {
                            osAssetMap[file["asset_id"]] = {
                                name: file["filename"],
                                url: file["url"]
                            };

                            if (!file["downloaded"] && (devMode || !file['filename'].match(/-v?(4\.|5\.[0-4])/))) {
                                $('#OSSelect').append($('<option>', {
                                    value: file["asset_id"],
                                    text: file["filename"] + " (download)"
                                }));
                            }
                            if (file["downloaded"]) {
                                $('#OSSelect').append($('<option>', {
                                    value: file["asset_id"],
                                    text: file["filename"]
                                }));
                            }
                        }
                    }

                    //handle what age OS to display
                    if ($('#LegacyOS').is(':checked')) {
                        //leave all avail options in place
                    } else {
                        //remove legacy files (n-1) - git assetid needs manually updating over time
                        $('#OSSelect option').filter(function () { return parseInt(this.value) < 103024154; }).remove();
                    }

                    //only show alpha and beta images in Advanced ui
                    if (settings['uiLevel'] && (parseInt(settings['uiLevel']) >= 1)) {
                        //leave all avail options in place
                    } else {
                        $('#OSSelect option').filter(function () { return (/beta/i.test(this.text)); }).remove(); //remove beta'sbin
                        $('#OSSelect option').filter(function () { return (/alpha/i.test(this.text)); }).remove(); //remove beta'sbin
                    }

                    //insert files already downloaded if we haven't got them from the git api call
                    var osUpdateFiles = <?php echo json_encode($osUpdateFiles); ?>;
                    var select = $('#OSSelect');
                    osUpdateFiles.forEach(element => {
                        if (select.has('option:contains("' + element + '")').length == 0) {
                            $('#OSSelect').append($('<option>', {
                                value: element,
                                text: element
                            }));
                        }
                    });
                    showHideOsSelect();
                }); <?
            } ?>
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
            $.get('api/system/status', function (data) {
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
                    remoteVer +=
                        " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
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

            //set os name (url for download)
            if (os in osAssetMap) {
                osName = osAssetMap[os].name;
                os = osAssetMap[os].url;
            }

            //override file location from git to local if already downloaded
            if ($('#OSSelect option:selected').text().toLowerCase().indexOf('(download)') === -1) {
                os = $('#OSSelect option:selected').text();
                osName = $('#OSSelect option:selected').text();
            }

            if (confirm('Upgrade the OS using ' + osName +
                '?\nThis can take a long time. It is also strongly recommended to run FPP backup first.')) {

                var options = {
                    id: "upgradeOSPopupStatus",
                    title: "FPP OS Upgrade",
                    body: "<textarea style='max-width:100%; max-height:100%; width: 100%; height:100%;' disabled id='streamedUpgradeOSText'></textarea>",
                    class: "modal-dialog-scrollable",
                    noClose: true,
                    keyboard: false,
                    backdrop: "static",
                    footer: "",
                    buttons: {
                        "Close": {
                            id: 'fppUpgradeOSCloseDialogButton',
                            click: function () {
                                CloseModalDialog("upgradeOSPopupStatus");
                                location.reload();
                            },
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
                    body: "<textarea style='max-width:100%; max-height:100%; width: 100%; height:100%;' disabled id='streamedUDownloadText'></textarea>",
                    class: "modal-dialog-scrollable",
                    noClose: true,
                    keyboard: false,
                    backdrop: "static",
                    footer: "",
                    buttons: {
                        "Close": {
                            id: 'fppDownloadCloseDialogButton',
                            click: function () {
                                CloseModalDialog("downloadPopupStatus");
                                location.reload();
                            },
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
                echo "os = '';\n";
            } ?>
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
                body: "<textarea style='max-width:100%; max-height:100%; width: 100%; height:100%;' disabled id='streamedUpgradeText'></textarea>",
                class: "modal-dialog-scrollable",
                noClose: true,
                keyboard: false,
                backdrop: "static",
                footer: "",
                buttons: {
                    "Close": {
                        id: 'fppUpgradeCloseDialogButton',
                        click: function () {
                            CloseFPPUpgradeDialog();
                        },
                        disabled: true,
                        class: 'btn-success'
                    }
                }
            };
            $("#fppUpgradeCloseDialogButton").prop("disabled", true);
            DoModalDialog(options);
            StreamURL('manualUpdate.php?wrapped=1', 'streamedUpgradeText', 'FPPUpgradeDone');
        }


    </script>
    <title>
        <? echo $pageTitle; ?>
    </title>
    <style>
        .no-close .ui-dialog-titlebar-close {
            display: none
        }

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

        h4,
        h3 {
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
        <?
        $activeParentMenuItem = 'help';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <div class="title">About FPP</div>
            <div class="pageContent">

                <div>
                    <div style="overflow: hidden; padding: 10px;">
                        <div class="row">
                            <div class="aboutLeft col-md">
                                <table class='tblAbout'>
                                    <tr>
                                        <td><i class="fa-solid fa-tag fa-fw"></i><b>Version Info</b></td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td>FPP Version:</td>
                                        <td id='fppVersion'>
                                            <? echo $fpp_version; ?>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Platform:</td>
                                        <td>
                                            <?
                                            echo $settings['Platform'];
                                            if (($settings['Variant'] != '') && ($settings['Variant'] != $settings['Platform'])) {
                                                echo " (" . $settings['Variant'] . ")";
                                            }
                                            ?>
                                        </td>
                                    </tr>
                                    <? if ($os_build != "") { ?>
                                        <tr>
                                            <td>FPP OS Build:</td>
                                            <td id='osVersion'>
                                                <? echo $os_build; ?>
                                            </td>
                                        </tr>
                                    <? } ?>
                                    <tr>
                                        <td>OS Version:</td>
                                        <td id='osRelease'>
                                            <? echo $os_version; ?>
                                        </td>
                                    </tr>
                                    <? if (isset($serialNumber) && $serialNumber != "") { ?>
                                        <tr>
                                            <td>Hardware Serial Number:</td>
                                            <td>
                                                <? echo $serialNumber; ?>
                                            </td>
                                        </tr>
                                    <? } ?>
                                    <tr>
                                        <td>Kernel Version:</td>
                                        <td>
                                            <? echo $kernel_version; ?>
                                        </td>
                                    </tr>
                                    <? if ($lastBoot != "") { ?>
                                        <tr>
                                            <td>System Boot Time:</td>
                                            <td id='lastBoot'>
                                                <? echo $lastBoot; ?>
                                            </td>
                                        </tr>
                                    <? } ?>
                                    <tr>
                                        <td>fppd Uptime:</td>
                                        <td id='fppdUptime'></td>
                                    </tr>
                                    <tr>
                                        <td>Local Git Version:</td>
                                        <td id='localGitVersion'>
                                            <?
                                            echo $git_version;
                                            echo " <a href='changelog.php'>ChangeLog</a>";
                                            if (
                                                ($git_remote_version != "") &&
                                                ($git_remote_version != "Unknown") &&
                                                ($git_version != $git_remote_version)
                                            ) {
                                                echo " <b class='text-success'>(Update is available)</b>";
                                            }

                                            ?>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Remote Git Version:</td>
                                        <td id='remoteGitVersion'>
                                            <?
                                            echo $git_remote_version;
                                            if (
                                                ($git_remote_version != "") &&
                                                ($git_remote_version != "Unknown") &&
                                                ($git_version != $git_remote_version)
                                            ) {
                                                echo " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
                                            }

                                            ?>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Upgrade FPP:</td>
                                        <td><input type='button' value='Upgrade FPP' onClick='UpgradeFPP();'
                                                class='buttons btn-outline-success' id='ManualUpdate'></td>
                                    </tr>
                                    <?
                                    if ($settings['uiLevel'] > 0) {
                                        $upgradeSources = array();
                                        $remotes = getKnownFPPSystems();

                                        if ($settings["Platform"] != "MacOS") {
                                            $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));
                                        } else {
                                            $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | grep 'inet ' | awk '{print \$2}'")));
                                        }
                                        $found = 0;
                                        foreach ($remotes as $desc => $host) {
                                            if ((!in_array($host, $IPs)) && (!preg_match('/^169\.254\./', $host))) {
                                                $upgradeSources[$desc] = $host;
                                                if (isset($settings['UpgradeSource']) && ($settings['UpgradeSource'] == $host)) {
                                                    $found = 1;
                                                }

                                            }
                                        }
                                        if (!$found && isset($settings['UpgradeSource']) && ($settings['UpgradeSource'] != 'github.com')) {
                                            $upgradeSources = array($settings['UpgradeSource'] . ' (Unreachable)' => $settings['UpgradeSource'], 'github.com' => 'github.com') + $upgradeSources;
                                        } else {
                                            $upgradeSources = array("github.com" => "github.com") + $upgradeSources;
                                        }

                                        ?>
                                        <tr>
                                            <td>FPP Upgrade Source:</td>
                                            <td>
                                                <? PrintSettingSelect("Upgrade Source", "UpgradeSource", 0, 0, "github.com", $upgradeSources); ?>
                                            </td>
                                        </tr>
                                        <?
                                    }
                                    ?>

                                    <tr id='osSelectRow'>
                                        <td style='vertical-align: top;'>Upgrade OS:</td>
                                        <td><select class='OSSelect' id='OSSelect' onChange='OSSelectChanged();'>
                                                <option value=''>-- Choose an OS Version --</option>
                                            </select>

                                            <?
                                            if ($settings['uiLevel'] > 0) {
                                                echo "<br><span><i class='fas fa-fw fa-graduation-cap ui-level-1'></i> Show All Platforms <input type='checkbox' id='allPlatforms' onClick='PopulateOSSelect();'><img id='allPlatforms_img' title='Show both BBB & Pi downloads' src='images/redesign/help-icon.svg' class='icon-help'></span>";
                                            }

                                            if ($settings['uiLevel'] > 0) {
                                                echo "<br><span><i class='fas fa-fw fa-timeline ui-level-1'></i> Show Legacy OS's <input type='checkbox' id='LegacyOS' onClick='PopulateOSSelect();'><img id='allOSs_img' title='Included Historic OS releases in listing' src='images/redesign/help-icon.svg' class='icon-help'></span>";
                                            }

                                            if ($settings['uiLevel'] == 3) {
                                                echo "<br><span><i class='fas fa-fw fa-code ui-level-3'></i> Preserve /opt/fpp <input type='checkbox' id='keepOptFPP'><img id='keepOptFPP_img' title='WARNING: This will upgrade the OS but will not upgrade the FPP version.  This may cause issues if the versions are not compatible.' src='images/redesign/help-icon.svg' class='icon-help'></span>";
                                            }

                                            echo "<br><input type='button' disabled value='Upgrade OS' onClick='UpgradeOS();' class='buttons' id='OSUpgrade'>&nbsp;<input type='button' disabled value='Download Only' onClick='DownloadOS();' class='buttons' id='OSDownload'></td></tr>";
                                            ?>
                                    <tr>
                                        <td>&nbsp;</td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td><i class="fa-solid fa-chart-line fa-fw"></i><b>System Utilization</b></td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td>CPU Usage:</td>
                                        <td>
                                            <? printf("%.2f", get_server_cpu_usage()); ?>%
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Memory Usage:</td>
                                        <td>
                                            <? printf("%.2f", get_server_memory_usage()); ?>%
                                        </td>
                                    </tr>
                                    <tr>
                                        <td style="vertical-align:top;">Sensors:</td>
                                        <td>
                                            <div id="sensorData"></div>
                                        </td>
                                    </tr>

                                    <tr>
                                        <td>&nbsp;</td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td><i class="fa-regular fa-clock fa-fw"></i><b>Uptime</b></td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td colspan='2'>
                                            <? echo $uptime; ?>
                                        </td>
                                    </tr>

                                </table>
                                <br />
                            </div>
                            <div class="aboutRight col-md">
                                <table class='tblAbout'>
                                    <tr>
                                        <td><i class="fa-solid fa-chart-simple fa-fw"></i><b>Player Stats</b></td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <tr>
                                        <td>Schedules:</td>
                                        <td><a href='scheduler.php' class='nonULLink' id='scheduleCount'></a></td>
                                    </tr>
                                    <tr>
                                        <td>Playlists:</td>
                                        <td><a href='playlists.php' class='nonULLink'>
                                                <? echo getFileCount($playlistDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <tr>
                                        <td>Sequences:</td>
                                        <td><a href='filemanager.php' class='nonULLink'>
                                                <? echo getFileCount($sequenceDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <tr>
                                        <td>Audio Files:</td>
                                        <td><a href='filemanager.php#tab-audio' class='nonULLink'>
                                                <? echo getFileCount($musicDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <tr>
                                        <td>Videos:</td>
                                        <td><a href='filemanager.php#tab-video' class='nonULLink'>
                                                <? echo getFileCount($videoDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <tr>
                                        <td>Command Presets:</td>
                                        <td><a href='commandPresets.php' class='nonULLink' id='commandPresetCount'></a>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Effects:</td>
                                        <td><a href='filemanager.php#tab-effects' class='nonULLink'>
                                                <? echo getFileCount($effectDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <tr>
                                        <td>Scripts:</td>
                                        <td><a href='filemanager.php#tab-scripts' class='nonULLink'>
                                                <? echo getFileCount($scriptDirectory); ?>
                                            </a></td>
                                    </tr>
                                    <?
                                    if (file_exists($pluginDirectory)) {
                                        $handle = opendir($pluginDirectory);
                                        while (($plugin = readdir($handle)) !== false) {
                                            if (!in_array($plugin, array('.', '..'))) {
                                                $pluginInfoFile = $pluginDirectory . "/" . $plugin . "/pluginInfo.json";
                                                if (file_exists($pluginInfoFile)) {
                                                    $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
                                                    if (isset($pluginConfig["fileExtensions"])) {
                                                        foreach ($pluginConfig["fileExtensions"] as $key => $value) {
                                                            if (isset($value["tab"]) && isset($value["folder"])) {
                                                                $folder = $mediaDirectory . "/" . $value["folder"];
                                                                ?>
                                                                <tr>
                                                                    <td><?= $value["tab"] ?>:</td>
                                                                    <td><a href='filemanager.php#tab-<? $key ?>'
                                                                            class='nonULLink'><? echo getFileCount($folder); ?></a></td>
                                                                </tr>
                                                                <?
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        closedir($handle);
                                    }
                                    ?>
                                    <tr>
                                        <td>&nbsp;</td>
                                        <td>&nbsp;</td>
                                    </tr>

                                    <tr>
                                        <td><i class="fa-regular fa-hard-drive fa-fw"></i><b>Disk Utilization</b></td>
                                        <td>&nbsp;</td>
                                    </tr>
                                    <?
                                    $diskTotal = disk_total_space("/");
                                    $diskFree = disk_free_space("/");
                                    $percentageUsed = 100 - ($diskFree * 100 / $diskTotal);
                                    $progressClass = "bg-success";
                                    if ($percentageUsed > 60) {
                                        $progressClass = "bg-warning";
                                    }
                                    if ($percentageUsed > 80) {
                                        $progressClass = "bg-danger";
                                    }
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
                                                <div class="progress-bar <? echo $progressClass; ?>" role="progressbar"
                                                    style="width: <? printf(" %2.0f%%", $percentageUsed); ?>;"
                                                    aria-valuenow="25" aria-valuemin="0" aria-valuemax="100">
                                                    <? printf("%2.0f%%", $percentageUsed); ?>
                                                </div>
                                            </div>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>Root
                                            <? if ($rootDevice != "") {
                                                echo "(" . $rootDevice . ")";
                                            }
                                            ?> Free Space:
                                        </td>
                                        <td>

                                            <?
                                            printf("%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
                                            ?>
                                        </td>
                                    </tr>
                                    <?
                                    if (isset($mediaDevice) && $mediaDevice != "" && $mediaDevice != $rootDevice) {
                                        $diskTotal = disk_total_space($mediaDirectory);
                                        $diskFree = disk_free_space($mediaDirectory);
                                        $percentageUsed = 100 - ($diskFree * 100 / $diskTotal);
                                        $progressClass = "bg-success";
                                        if ($percentageUsed > 60) {
                                            $progressClass = "bg-warning";
                                        }
                                        if ($percentageUsed > 80) {
                                            $progressClass = "bg-danger";
                                        }
                                        ?>
                                        <tr>
                                            <td colspan="2">
                                                <div class="progress mt-2">
                                                    <div class="progress-bar <? echo $progressClass; ?>" role="progressbar"
                                                        style="width:<? printf(" %2.0f%%", $percentageUsed); ?>;"
                                                        aria-valuenow="25" aria-valuemin="0" aria-valuemax="100">
                                                        <? printf("%2.0f%%", $percentageUsed); ?>
                                                    </div>
                                                </div>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>Media (
                                                <? echo $mediaDevice; ?>
                                                ) Free Space:
                                            </td>
                                            <td>
                                                <?
                                                printf("%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
                                                ?>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td></td>
                                            <td></td>
                                        </tr>
                                        <?
                                    } ?>

                                    <?
                                    if (!isset($settings['cape-info']) || !isset($settings['cape-info']['verifiedKeyId']) || ($settings['cape-info']['verifiedKeyId'] != 'fp')) {
                                        ?>
                                        <tr>
                                            <td style='height: 50px;'></td>
                                        </tr>
                                        <tr>
                                            <td colspan='2' style='width: 300px;'>
                                                If you would like to donate to the Falcon Player development team to help
                                                support the continued development of FPP, you can use the donate button
                                                below.<br>
                                                <form action="https://www.paypal.com/donate" method="post" target="_top">
                                                    <input type="hidden" name="hosted_button_id"
                                                        value="ASF9XYZ2V2F5G" /><input style="height: 60px;" type="image"
                                                        src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif"
                                                        border="0" name="submit" title="Donate to the Falcon Player"
                                                        alt="Donate to the Falcon Player" /><img alt="" border="0"
                                                        src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1"
                                                        height="1" />
                                                </form>
                                            </td>
                                        </tr>
                                        <?
                                    }
                                    ?>
                                </table>
                            </div>
                            <div class="clear"></div>

                        </div>
                        <?
                        $eepromFile = "/home/fpp/media/tmp/eeprom.bin";
                        if (!file_exists($eepromFile) && file_exists("/home/fpp/media/config/cape-eeprom.bin")) {
                            $eepromFile = "/home/fpp/media/config/cape-eeprom.bin";
                        }
                        if (file_exists($eepromFile)) {
                            echo "<hr>For Cape information and EEPROM signing, go to the <a href='cape-info.php'>Cape Info</a> page.\n";
                        }
                        ?>

                    </div>
                </div>
            </div>
        </div>
        <? include 'common/footer.inc'; ?>
    </div>
</body>

</html>