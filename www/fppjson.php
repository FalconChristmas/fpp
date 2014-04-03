<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('commandsocket.php');

//define('debug', true);

$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
	"getChannelRemaps"    => 'GetChannelRemaps',
	"setChannelRemaps"    => 'SetChannelRemaps',
	"getChannelOutputs"   => 'GetChannelOutputs',
	"setChannelOutputs"   => 'SetChannelOutputs',
	"getSetting"          => 'GetSetting',
	"setSetting"          => 'SetSetting'
);

$command = "";
$args = Array();

if ( isset($_GET['command']) && !empty($_GET['command']) ) {
	$command = $_GET['command'];
	$args = $_GET;
} else if ( isset($_POST['command']) && !empty($_POST['command']) ) {
	$command = $_POST['command'];
	$args = $_POST;
}

if (array_key_exists($command,$command_array) )
{
	if ( defined('debug') )
		error_log("Calling " .$command);

	call_user_func($command_array[$command]);
}
return;

/////////////////////////////////////////////////////////////////////////////

function returnJSON($arr) {
	header( "Content-Type: application/json");

	echo json_encode($arr);

	exit(0);
}

function check($var)
{
	if ( empty($var) || !isset($var) )
	{
		error_log("WARNING: Variable we checked in function '".$_GET['command']."' was empty");
//		die();
	}
}

/////////////////////////////////////////////////////////////////////////////

function GetSetting()
{
	global $args;

	$setting = $args['key'];
	check($setting);

	$value = ReadSettingFromFile($setting);

	$result = Array();
	$result[$setting] = $value;

	returnJSON($result);
}

function SetSetting()
{
	global $args;

	$setting = $args['key'];
	$value   = $args['value'];

	check($setting);
	check($value);

	WriteSettingToFile($setting, $value);

	if ($setting == "LogLevel") {
		SendCommand("LogLevel,$value,");
	} else if ($setting == "LogMask") {
		$newValue = "";
		if ($value != "")
			$newValue = preg_replace("/,/", ";", $value);
		SendCommand("LogMask,$newValue,");
	}

	GetSetting();
}

/////////////////////////////////////////////////////////////////////////////

function GetChannelRemaps()
{
	global $remapFile;

	$result = Array();

	$f = fopen($remapFile, "r");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	while (!feof($f))
	{
		$line = trim(fgets($f));

		if ($line == "")
			continue;
		
		if (substr($line, 0, 1) == "#")
			continue;

		$remap = explode(",",$line,10);

		$elem = Array();
		$elem['Source'] = $remap[0];
		$elem['Destination'] = $remap[1];
		$elem['Count'] = $remap[2];

		$result[] = $elem;
	}
	fclose($f);

	returnJSON($result);
}

function SetChannelRemaps()
{
	global $remapFile;
	global $args;

	$data = json_decode($args['data'], true);

	$f = fopen($remapFile, "w");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	foreach ($data as $remap) {
		fprintf($f, "%d,%d,%d\n",
			$remap['Source'], $remap['Destination'], $remap['Count']);
	}
	fclose($f);

	GetChannelRemaps();
}

/////////////////////////////////////////////////////////////////////////////
function GetChannelOutputs()
{
	global $settings;

	$result = Array();
	$result['Outputs'] = Array();

	$f = fopen($settings['channelOutputsFile'], "r");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	while (!feof($f))
	{
		$line = trim(fgets($f));

		if ($line == "")
			continue;

		array_push($result['Outputs'], $line);
	}
	fclose($f);

	return returnJSON($result);
}

function SetChannelOutputs()
{
	global $settings;
	global $args;

	$data = json_decode($args['data'], true);

	$f = fopen($settings['channelOutputsFile'], "w");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	foreach ($data['Outputs'] as $output) {
		fprintf($f, "%s\n", $output);
	}
	fclose($f);

	GetChannelOutputs();
}

/////////////////////////////////////////////////////////////////////////////

?>
