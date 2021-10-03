<?php

$skipJSsettings = 1;
require_once 'common.php';

require_once 'universeentry.php';
require_once 'pixelnetdmxentry.php';
require_once 'commandsocket.php';

error_reporting(E_ALL);

// Commands defined here which return something other
// than XML need to return their own Content-type header.
$nonXML = array(
    "viewRemoteScript" => 1,
);

$a = session_id();
if (empty($a)) {
    session_start();
}
$_SESSION['session_id'] = session_id();

$command_array = array(
    //"getFiles" => 'GetFiles', // /api/files/:dirName
    //"getZip" => 'GetZip',
    //"getUniverseReceivedBytes" => 'GetUniverseReceivedBytes',  // GET /api/channel/input/stats
    // "deleteFile" => 'DeleteFile', // use DELETE /api/file/:DirName/:filename
    //"setUniverseCount" => 'SetUniverseCount',
    //"getUniverses" => 'GetUniverses',
    // "getPixelnetDMXoutputs" => 'GetPixelnetDMXoutputs', // GET /api/channel/output/PixelnetDMX
    // "deleteUniverse" => 'DeleteUniverse', // Moved to UI
    // "cloneUniverse" => 'CloneUniverse', //Moved to UI
    // "viewReleaseNotes" => 'ViewReleaseNotes',  // use GET /api/system/releaseNotes/:version
    // "viewRemoteScript" => 'ViewRemoteScript', // GET /api/scripts/viewRemote/:category/:filename
    //"installRemoteScript" => 'InstallRemoteScript', //GET /api/script/install/:category/:filename
    "moveFile" => 'MoveFile', // DEPRECATED. Saved for xLights uploads
    //"isFPPDrunning" => 'IsFPPDrunning', // GET /api/system/status
    // "getFPPstatus" => 'GetFPPstatus', use GET /api/fppd/status instead
    // "stopGracefully" => 'StopGracefully', use api/playlists/stopgracefully or command API
    // "stopGracefullyAfterLoop" => 'StopGracefullyAfterLoop', use api/playlists/stopgracefullyafterloop or Command API
    // "stopNow" => 'StopNow', // api/playlists/stop or Command API
    // "stopFPPD" => 'StopFPPD', // use GET /api/system/fppd/stop
    // "startFPPD" => 'StartFPPD', // use GET /api/system/fppd/start
    "restartFPPD" => 'RestartFPPD', // retained for xLights and Multisync
    // "startPlaylist" => 'StartPlaylist',  // use Command API Instead
    "rebootPi" => 'RebootPi', // Used my MultiSync
    "shutdownPi" => 'ShutdownPi', // Kept for Multisync
    //"changeGitBranch" => 'ChangeGitBranch', // Deprecated use changebranch.php?
    // "upgradeFPPVersion" => 'UpgradeFPPVersion', Replaced by upgradefpp.php?
    //"gitStatus" => 'GitStatus', // use GET /api/git/status instead
    // "resetGit" => 'ResetGit', // use GET /api/git/reset
    // "setVolume" => 'SetVolume', // use POST /api/system/volume
    "setFPPDmode" => 'SetFPPDmode', // Legacy. used by MultiSync Future should use PUT /api/settings/fppMode
    // "getVolume" => 'GetVolume', // use GET /api/system/volume
    //"getBridgeInputDelayBeforeBlack" => 'GetBridgeInputDelayBeforeBlack', // Replaced by /api/settings/
    //"setBridgeInputDelayBeforeBlack" =>'SetBridgeInputDelayBeforeBlack', // Replaced by /api/settings/
    //"getFPPDmode" => 'GetFPPDmode', // Replaced by /api/settings/fppMode
    //"playEffect" => 'PlayEffect', // Use Command API
    //"stopEffect" => 'StopEffect', // Use Command API
    //"stopEffectByName" => 'StopEffectByName', // Use Command API
    //"deleteEffect" => 'DeleteEffect', // never implemented
    //"getRunningEffects" => 'GetRunningEffects',
    //"triggerEvent" => 'TriggerEvent', // DEPRECATED
    //"saveEvent" => 'SaveEvent', // DEPRECATED
    //"deleteEvent" => 'DeleteEvent', // DEPRECATED
    //"getFile" => 'GetFile', // Replaced by /api/file/
    //"tailFile" => 'TailFile', // Replaced by api/file
    //"saveUSBDongle" => 'SaveUSBDongle', replaced by PUT /api/settings/
    // "getInterfaceInfo" => 'GetInterfaceInfo',  // Never used
    //"setupExtGPIO" => 'SetupExtGPIO',
    //"extGPIO" => 'ExtGPIO'
);

if (isset($_GET['command']) && !isset($nonXML[$_GET['command']])) {
    header('Content-type: text/xml');
}

if (isset($_GET['command']) && !empty($_GET['command'])) {
    global $debug;

    if (array_key_exists($_GET['command'], $command_array)) {
        if ($debug) {
            error_log("Calling " . $_GET['command']);
        }

        call_user_func($command_array[$_GET['command']]);
    }
    return;
}

/////////////////////////////////////////////////////////////////////////////

function EchoStatusXML($status)
{
    $doc = new DomDocument('1.0');
    $root = $doc->createElement('Status');
    $root = $doc->appendChild($root);
    $value = $doc->createTextNode($status);
    $value = $root->appendChild($value);
    echo $doc->saveHTML();
}

/////////////////////////////////////////////////////////////////////////////

function RebootPi()
{
    global $SUDO;

    $status = exec($SUDO . " shutdown -r now");

    header("Access-Control-Allow-Origin: *");
    EchoStatusXML($status);
}

function SetFPPDmode()
{
    $mode_string['0'] = "unknown";
    $mode_string['1'] = "bridge";
    $mode_string['2'] = "player";
    $mode_string['6'] = "master";
    $mode_string['8'] = "remote";
    $mode = $_GET['mode'];
    check($mode, "mode", __FUNCTION__);
    WriteSettingToFile("fppMode", $mode_string["$mode"]);
    EchoStatusXML("true");
}

function ShutdownPi()
{
    global $SUDO;

    $status = exec($SUDO . " shutdown -h now");
    EchoStatusXML($status);
}

function MoveFile()
{
    global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory, $imageDirectory, $configDirectory, $SUDO;

    $file = sanitizeFilename($_GET['file']);
    check($file, "file", __FUNCTION__);

    // Fix double quote uploading by simply moving the file first, if we find it with URL encoding
    if (strstr($file, '"')) {
        if (!rename($uploadDirectory . "/" . preg_replace('/"/', '%22', $file), $uploadDirectory . "/" . $file)) {
            //Firefox and xLights will upload with " intact so if the rename doesn't work, it's OK
        }
    }

    if (file_exists($uploadDirectory . "/" . $file)) {
        if (preg_match("/\.(fseq)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $sequenceDirectory . '/' . $file)) {
                error_log("Couldn't move sequence file");
                exit(1);
            }
        } else if (preg_match("/\.(fseq.gz)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $sequenceDirectory . '/' . $file)) {
                error_log("Couldn't move sequence file");
                exit(1);
            }
            $nfile = $file;
            $nfile = str_replace('"', '\\"', $nfile);
            exec("$SUDO gunzip -f \"$sequenceDirectory/$nfile\"");
        } else if (preg_match("/\.(eseq)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $effectDirectory . '/' . $file)) {
                error_log("Couldn't move effect file");
                exit(1);
            }
        } else if (preg_match("/\.(mp4|mkv|avi|mov|mpg|mpeg)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $videoDirectory . '/' . $file)) {
                error_log("Couldn't move video file");
                exit(1);
            }
        } else if (preg_match("/\.(gif|jpg|jpeg|png)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $imageDirectory . '/' . $file)) {
                error_log("Couldn't move image file");
                exit(1);
            }
        } else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $file)) {
            // Get rid of any DOS newlines
            $contents = file_get_contents($uploadDirectory . "/" . $file);
            $contents = str_replace("\r", "", $contents);
            file_put_contents($uploadDirectory . "/" . $file, $contents);

            if (!rename($uploadDirectory . "/" . $file, $scriptDirectory . '/' . $file)) {
                error_log("Couldn't move script file");
                exit(1);
            }
        } else if (preg_match("/\.(mp3|ogg|m4a|wav|au|m4p|wma|flac)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $musicDirectory . '/' . $file)) {
                error_log("Couldn't move music file");
                exit(1);
            }
        } else if (preg_match("/eeprom\.bin$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $configDirectory . '/cape-eeprom.bin')) {
                error_log("Couldn't move eeprom file");
                exit(1);
            }
        }
    } else {
        error_log("Couldn't find file '" . $file . "' in upload directory");
        exit(1);
    }
    EchoStatusXML('Success');
}

// This old method is for xLights and multisync
function RestartFPPD()
{
    header("Access-Control-Allow-Origin: *");
    $url = "http://localhost/api/system/fppd/restart";

    if ((isset($_GET['quick'])) && ($_GET['quick'] == 1)) {
        $url = $url . "?quick=1";
    }

    $curl = curl_init($url);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    $request_content = curl_exec($curl);
    EchoStatusXML('true');
}
