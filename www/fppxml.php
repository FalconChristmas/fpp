<?php

$skipJSsettings = 1;
require_once('common.php');

require_once('universeentry.php');
require_once('pixelnetdmxentry.php');
require_once('commandsocket.php');

error_reporting(E_ALL);

// Commands defined here which return something other
// than XML need to return their own Content-type header.
$nonXML = Array(
	"viewRemoteScript" => 1
	);

$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
	//"getFiles" => 'GetFiles', // /api/files/:dirName
	//"getZip" => 'GetZip',
	//"getUniverseReceivedBytes" => 'GetUniverseReceivedBytes',  // GET /api/channel/input/stats
	// "deleteFile" => 'DeleteFile', // use DELETE /api/file/:DirName/:filename
	//"setUniverseCount" => 'SetUniverseCount', 
	//"getUniverses" => 'GetUniverses',
	"getPixelnetDMXoutputs" => 'GetPixelnetDMXoutputs',
	"deleteUniverse" => 'DeleteUniverse',
	"cloneUniverse" => 'CloneUniverse',
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
	"setFPPDmode" => 'SetFPPDmode', // Legacy. Should use PUT /api/settings/fppMode
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

if (isset($_GET['command']) && !isset($nonXML[$_GET['command']]))
	header('Content-type: text/xml');



if ( isset($_GET['command']) && !empty($_GET['command']) )
{
	global $debug;

	if ( array_key_exists($_GET['command'],$command_array) )
	{
		if ($debug)
			error_log("Calling ".$_GET['command']);
		call_user_func($command_array[$_GET['command']]);
	}
	return;
}
else if(!empty($_POST['command']) && $_POST['command'] == "saveHardwareConfig")
{
	SaveHardwareConfig();
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

	$status=exec($SUDO . " shutdown -r now");

    header( "Access-Control-Allow-Origin: *");
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
	WriteSettingToFile("fppMode",$mode_string["$mode"]);
	EchoStatusXML("true");
}

function ShutdownPi()
{
	global $SUDO;

	$status=exec($SUDO . " shutdown -h now");
	EchoStatusXML($status);
}

function MoveFile()
{
	global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory, $imageDirectory, $configDirectory, $SUDO;

	$file = $_GET['file'];
	check($file, "file", __FUNCTION__);

	// Fix double quote uploading by simply moving the file first, if we find it with URL encoding
	if ( strstr($file, '"') ) {
		if (!rename($uploadDirectory."/" . preg_replace('/"/', '%22', $file), $uploadDirectory."/" . $file)) {
            //Firefox and xLights will upload with " intact so if the rename doesn't work, it's OK
		}
	}
    
	if (file_exists($uploadDirectory."/" . $file)) {
		if (preg_match("/\.(fseq)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $sequenceDirectory . '/' . $file) ) {
				error_log("Couldn't move sequence file");
				exit(1);
			}
		} else if (preg_match("/\.(fseq.gz)$/i", $file)) {
            if ( !rename($uploadDirectory."/" . $file, $sequenceDirectory . '/' . $file) ) {
                error_log("Couldn't move sequence file");
                exit(1);
            }
            $nfile = $file;
            $nfile = str_replace('"', '\\"', $nfile);
            exec("$SUDO gunzip -f \"$sequenceDirectory/$nfile\"");
        } else if (preg_match("/\.(eseq)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $effectDirectory . '/' . $file) ) {
				error_log("Couldn't move effect file");
				exit(1);
			}
		} else if (preg_match("/\.(mp4|mkv|avi|mov|mpg|mpeg)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $videoDirectory . '/' . $file) ) {
				error_log("Couldn't move video file");
				exit(1);
			}
		} else if (preg_match("/\.(gif|jpg|jpeg|png)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $imageDirectory . '/' . $file) ) {
				error_log("Couldn't move image file");
				exit(1);
			}
		} else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $file)) {
			// Get rid of any DOS newlines
			$contents = file_get_contents($uploadDirectory."/".$file);
			$contents = str_replace("\r", "", $contents);
			file_put_contents($uploadDirectory."/".$file, $contents);

			if ( !rename($uploadDirectory."/" . $file, $scriptDirectory . '/' . $file) ) {
				error_log("Couldn't move script file");
				exit(1);
			}
        } else if (preg_match("/\.(mp3|ogg|m4a|wav|au|m4p|wma|flac)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $musicDirectory . '/' . $file) ) {
				error_log("Couldn't move music file");
				exit(1);
			}
        } else if (preg_match("/eeprom\.bin$/i", $file)) {
            if ( !rename($uploadDirectory."/" . $file, $configDirectory . '/cape-eeprom.bin') ) {
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
	header( "Access-Control-Allow-Origin: *");
	$url = "http://localhost/api/system/fppd/restart";

    if ((isset($_GET['quick'])) && ($_GET['quick'] == 1))
    {
		$url = $url + "?quick=1";
	}

	$curl = curl_init($url);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    $request_content = curl_exec($curl);
	EchoStatusXML('true');
}


function GetLocalTime()
{
	return exec("date");
}

function SaveHardwareConfig()
{
	if (!isset($_POST['model']))
	{
		EchoStatusXML('Failure, no model supplied');
		return;
	}

	$model = $_POST['model'];
	$firmware = $_POST['firmware'];

	if ($model == "F16V2-alpha")
	{
		SaveF16v2Alpha();
	}
	else if ($model == "FPDv1")
	{
		SaveFPDv1();
	}
	else
	{
		EchoStatusXML('Failure, unknown model: ' . $model);
		return;
	}
	EchoStatusXML('Success');
}

function SaveF16v2Alpha()
{
    global $settings;
		$outputCount = 16;
  
		$carr = array();
		for ($i = 0; $i < 1024; $i++)
		{
			$carr[$i] = 0x0;
		}

		$i = 0;

		// Header
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0xCD;

		// Some byte
		$carr[$i++] = 0x01;


		for ($o = 0; $o < $outputCount; $o++)
		{
			$nodeCount = $_POST['nodeCount'][$o];
			$carr[$i++] = intval($nodeCount % 256);
			$carr[$i++] = intval($nodeCount / 256);

			$startChannel = $_POST['startChannel'][$o] - 1; // 0-based values in config file
			$carr[$i++] = intval($startChannel % 256);
			$carr[$i++] = intval($startChannel / 256);

			// Node Type is set on groups of 4 ports
			$carr[$i++] = intval($_POST['nodeType'][intval($o / 4) * 4]);

			$carr[$i++] = intval($_POST['rgbOrder'][$o]);
			$carr[$i++] = intval($_POST['direction'][$o]);
			$carr[$i++] = intval($_POST['groupCount'][$o]);
			$carr[$i++] = intval($_POST['nullNodes'][$o]);
		}

		$f = fopen($settings['configDirectory'] . "/Falcon.F16V2-alpha", "wb");
		fwrite($f, implode(array_map("chr", $carr)), 1024);
		fclose($f);
 		SendCommand('w');
}

function SaveFPDv1()
{
  global $settings;
  $outputCount = 12;

	$carr = array();
	for ($i = 0; $i < 1024; $i++)
	{
		$carr[$i] = 0x0;
	}

	$i = 0;
	// Header
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0xCC;

	$_SESSION['PixelnetDMXentries']=NULL;
	for ($o = 0; $o < $outputCount; $o++)
	{
    // Active Output
 		if( isset($_POST['FPDchkActive'][$o]))
		{
      $active = 1;
      $carr[$i++] = 1;
		}
		else
		{
      $active = 0;
      $carr[$i++] = 0;
		}
    // Start Address
    $startAddress = intval($_POST['FPDtxtStartAddress'][$o]);
    $carr[$i++] = $startAddress%256;
    $carr[$i++] = $startAddress/256;
    // Type
    $type = intval($_POST['pixelnetDMXtype'][$o]);
    $carr[$i++] = $type;
    $_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
  }
  $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "wb");
	fwrite($f, implode(array_map("chr", $carr)), 1024);

	fclose($f);
	SendCommand('w');
}

function CloneUniverse()
{
	$index = $_GET['index'];
	$numberToClone = $_GET['numberToClone'];
	check($index, "index", __FUNCTION__);
	check($numberToClone, "numberToClone", __FUNCTION__);

	if($index < count($_SESSION['UniverseEntries']) && ($index + $numberToClone) < count($_SESSION['UniverseEntries']))
	{
			$desc = $_SESSION['UniverseEntries'][$index]->desc;
			$universe = $_SESSION['UniverseEntries'][$index]->universe+1;
			$size = $_SESSION['UniverseEntries'][$index]->size;
			$startAddress = $_SESSION['UniverseEntries'][$index]->startAddress+$size;
			$type = $_SESSION['UniverseEntries'][$index]->type;
			$unicastAddress = $_SESSION['UniverseEntries'][$index]->unicastAddress;
			$priority = $_SESSION['UniverseEntries'][$index]->priority;

			for($i=$index+1;$i<$index+1+$numberToClone;$i++,$universe++)
			{
				 	$_SESSION['UniverseEntries'][$i]->desc	= $desc;
				 	$_SESSION['UniverseEntries'][$i]->universe	= $universe;
				 	$_SESSION['UniverseEntries'][$i]->size	= $size;
				 	$_SESSION['UniverseEntries'][$i]->startAddress	= $startAddress;
				 	$_SESSION['UniverseEntries'][$i]->type	= $type;
					$_SESSION['UniverseEntries'][$i]->unicastAddress	= $unicastAddress;
				 	$_SESSION['UniverseEntries'][$i]->priority	= $priority;
					$startAddress += $size;
 			}
	}
	EchoStatusXML('Success');
}

function DeleteUniverse()
{
	$index = $_GET['index'];
	check($index, "index", __FUNCTION__);

	if($index < count($_SESSION['UniverseEntries']) && count($_SESSION['UniverseEntries']) > 0 )
	{
		unset($_SESSION['UniverseEntries'][$index]);
		$_SESSION['UniverseEntries'] = array_values($_SESSION['UniverseEntries']);

	}
	EchoStatusXML('Success');
}

function LoadUniverseFile($input)
{
	global $settings;

	$_SESSION['UniverseEntries']=NULL;

	$filename = $settings['universeOutputs'];
	if ($input)
		$filename = $settings['universeInputs'];

	if(!file_exists($filename))
	{
		$_SESSION['UniverseEntries'][] = new UniverseEntry(1,"",1,1,512,0,"",0,0);
		return;
	}

	$jsonStr = file_get_contents($filename);

	$data = json_decode($jsonStr);
	$universes = 0;
	
	if ($input)
		$universes = $data->channelInputs[0]->universes;
	else
		$universes = $data->channelOutputs[0]->universes;

	foreach ($universes as $univ)
	{
		$active = $univ->active;
		$desc = $univ->description;
		$universe = $univ->id;
		$startAddress = $univ->startChannel;
		$size = $univ->channelCount;
		$type = $univ->type;
		$unicastAddress = $univ->address;
		$priority = $univ->priority;
		$_SESSION['UniverseEntries'][] = new UniverseEntry($active,$desc,$universe,$startAddress,$size,$type,$unicastAddress,$priority,0);
	}
}

function LoadPixelnetDMXFile()
{
  global $settings;

	$_SESSION['PixelnetDMXentries']=NULL;

  $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "rb");
	if($f == FALSE)
  {
  	fclose($f);
		//No file exists add one and save to new file.
		$address=1;
		for($i;$i<12;$i++)
		{
			$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry(1,0,$address);
			$address+=4096;
		}
		return;
  }

	$s = fread($f, 1024);
	fclose($f);
	$sarr = unpack("C*", $s);

	$dataOffset = 7;

	$i = 0;
	for ($i = 0; $i < 12; $i++)
	{
		$outputOffset  = $dataOffset + (4 * $i);
		$active        = $sarr[$outputOffset + 0];
		$startAddress  = $sarr[$outputOffset + 1];
		$startAddress += $sarr[$outputOffset + 2] * 256;
		$type          = $sarr[$outputOffset + 3];
		$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
  }
}

function SavePixelnetDMXoutputsToFile()
{
	global $pixelnetFile;

	$universes = "";
	$f=fopen($pixelnetFile,"w") or exit("Unable to open file! : " . $pixelnetFile);
	for($i=0;$i<count($_SESSION['PixelnetDMXentries']);$i++)
	{
			if($i==0)
			{
			$entries .= sprintf("%d,%d,%d,",
						$_SESSION['PixelnetDMXentries'][$i]->active,
						$_SESSION['PixelnetDMXentries'][$i]->type,
						$_SESSION['PixelnetDMXentries'][$i]->startAddress);
			}
			else
			{
			$entries .= sprintf("\n%d,%d,%d,",
						$_SESSION['PixelnetDMXentries'][$i]->active,
						$_SESSION['PixelnetDMXentries'][$i]->type,
						$_SESSION['PixelnetDMXentries'][$i]->startAddress);
			}

	}
	fwrite($f,$entries);
	fclose($f);

	EchoStatusXML('Success');
}



function GetPixelnetDMXoutputs()
{
	$reload = $_GET['reload'];
	check($reload, "reload", __FUNCTION__);

	if($reload == "TRUE")
	{
		LoadPixelnetDMXFile();
	}

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('PixelnetDMXentries');
	$root = $doc->appendChild($root);
	for($i=0;$i<count($_SESSION['PixelnetDMXentries']);$i++)
	{
		$PixelnetDMXentry = $doc->createElement('PixelnetDMXentry');
		$PixelnetDMXentry = $root->appendChild($PixelnetDMXentry);
		// active
		$active = $doc->createElement('active');
		$active = $PixelnetDMXentry->appendChild($active);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->active);
		$value = $active->appendChild($value);
		// type
		$type = $doc->createElement('type');
		$type = $PixelnetDMXentry->appendChild($type);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->type);
		$value = $type->appendChild($value);
		// startAddress
		$startAddress = $doc->createElement('startAddress');
		$startAddress = $PixelnetDMXentry->appendChild($startAddress);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->startAddress);
		$value = $startAddress->appendChild($value);
	}
	echo $doc->saveHTML();
}


function cmp_index($a, $b)
{
	if ($a->index == $b->index) {
		return 0;
	}
	return ($a->index < $b->index) ? -1 : 1;
}
    
function universe_cmp($a, $b)
{
    if ($a->startAddress == $b->startAddress) {
        return 0;
    }
    return ($a->startAddress < $b->startAddress) ? -1 : 1;
}

?>
