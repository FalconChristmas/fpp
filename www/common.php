<?php
require_once("config.php");

function check($var, $var_name = "", $function_name = "")
{
	global $debug;

	if ( empty($function_name) )
	{
		global $args;
		$function_name = $args['command'];
	}

	if ( empty($var) && $debug )
		error_log("WARNING: Variable '$var_name' in function '$function_name' was empty");
}

function ScrubFile($filename, $taboo = Array("emailgpass"))
{
	if ( !file_exists($filename) )
		return "";

	$dataStr = "";

	if (preg_match("/.json$/", $filename))
	{
		// Need to scrub .json as well at some point
		$dataStr = file_get_contents($filename);
	}
	else
	{
		$data = parse_ini_file($filename);

		foreach($taboo as $key)
			if (array_key_exists($key, $data))
				$data[$key] = "********";

		foreach ($data as $key => $value)
			$dataStr .= $key . " = " . $value . "\n";
	}

	return $dataStr;
}


function ReadSettingFromFile($settingName, $plugin = "")
{
	global $settingsFile;
	global $settings;
	$filename = $settingsFile;

	if (!file_exists($filename))
		return false;

	if ($plugin != "") {
		$filename = $settings["configDirectory"] . "/plugin." . $plugin;
	}

	$settingsStr = file_get_contents($filename);
  if ( !empty($settingsStr) )
  {
		if (preg_match("/^" . $settingName . "/m", $settingsStr))
		{
      $result = preg_match("/^" . $settingName . "\s*=(\s*\S*\w*)/m", $settingsStr, $output_array);
      if($result == 0)
      {
//        error_log("The setting " . $settingName . " could not be found in " . $filename);
        return false;
      }
      return trim($output_array[1], " \t\n\r\0\x0B\"");
		}
		else
		{
//      error_log("The setting " . $settingName . " could not be found in " . $filename);
			return false;
		}
  }
	else
	{
    error_log("The setting file:" . $filename . " could not be found." );
		return false;
	}
}

function WriteSettingToFile($settingName, $setting, $plugin = "")
{
	global $settingsFile;
	global $settings;
	$filename = $settingsFile;

	if ($plugin != "") {
		$filename = $settings['configDirectory'] . "/plugin." . $plugin;
	}

	$settingsStr = "";
	$tmpSettings = parse_ini_file($filename);
	$tmpSettings[$settingName] = $setting;
	foreach ($tmpSettings as $key => $value) {
		$settingsStr .= $key . " = \"" . $value . "\"\n";
	}
	file_put_contents($filename, $settingsStr);
}

function IfSettingEqualPrint($setting, $value, $print, $pluginName = "", $defaultValue = "")
{
	global $settings;
	global $pluginSettings;

	if ($pluginName != "") {
		if (($defaultValue != "") && !isset($pluginSettings[$setting]))
			$pluginSettings[$setting] = $defaultValue;

		if ((isset($pluginSettings[$setting])) &&
			($pluginSettings[$setting] == $value ))
			echo $print;
	} else {
		if (($defaultValue != "") && !isset($settings[$setting]))
			$settings[$setting] = $defaultValue;

		if ((isset($settings[$setting])) &&
			($settings[$setting] == $value ))
			echo $print;
	}
}

$settingGroups = Array();
$settingInfos = Array();
function LoadSettingInfos() {
    global $settings;
    global $settingInfos;
    global $settingGroups;

    if (empty($settingInfos) || empty($settingGroups)) {
        $data = json_decode(file_get_contents($settings['fppDir'] . '/www/settings.json'), true);
        $settingInfos = $data['settings'];
        $settingGroups = $data['settingGroups'];
    }
}

function PrintSetting($setting, $callback = '', $options = Array()) {
    global $settings;
    global $settingInfos;

    LoadSettingInfos();

    if (!isset($settingInfos[$setting])) {
        echo "<tr><td colspan='2'><b>Invalid Setting: $setting</b></td></tr>\n";
        return;
    }

    $s = $settingInfos[$setting];
    $level = isset($s['level']) ? $s['level'] : 0;
    $reloadUI = isset($s['reloadUI']) ? $s['reloadUI'] : 0;

    if (($callback == '') && ($reloadUI == 1))
        $callback = 'reloadSettingsPage';

    $checkFileOK = 0;
    if (isset($s['checkFile'])) {
        foreach ($s['checkFile'] as $f) {
            if (file_exists($f)) {
                $checkFileOK = 1;
            }
        }
    } else {
        $checkFileOK = 1;
    }

    if (($settings['uiLevel'] >= $level) &&
        ($checkFileOK) &&
        ((!isset($s['fppModes'])) ||
         (in_array('ALL', $s['fppModes'])) ||
         (in_array($settings['fppMode'], $s['fppModes']))) &&
        ((!isset($s['platforms'])) ||
         (in_array('ALL', $s['platforms'])) ||
         (in_array($settings['Platform'], $s['platforms'])))) {
        $restart = isset($s['restart']) ? $s['restart'] : 0;
        $reboot = isset($s['reboot']) ? $s['reboot'] : 0;

        echo "<tr><th>" . $s['desc'] . ":</th><td>";
        switch ($s['type']) {
            case 'select':
                if (empty($options)) {
                    if (isset($s['optionsURL'])) {
                        $json = "";
                        if (preg_match('/^\//', $s['optionsURL'])) {
                            $json = file_get_contents("http://" . $_SERVER['SERVER_ADDR'] . $s['optionsURL']);
                        } else {
                            $json = file_get_contents($s['optionsURL']);
                        }
                        $options = json_decode($json, true);
                    } else if (isset($s['options'])) {
                        $options = $s['options'];
                    }
                }

                $defaultValue = isset($s['defaultValue']) ? $s['defaultValue'] : "";

                PrintSettingSelect($s['desc'], $setting, $restart, $reboot, $defaultValue, $options, '', $callback);

                break;
            case 'checkbox':
                $checkedValue = isset($s['checkedValue']) ? $s['checkedValue'] : "1";
                $uncheckedValue = isset($s['uncheckedValue']) ? $s['uncheckedValue'] : "0";
                $defaultValue = isset($s['defaultValue']) ? $s['defaultValue'] : "0";

                PrintSettingCheckbox($s['desc'], $setting, $restart, $reboot, $checkedValue, $uncheckedValue, '', $callback, $defaultValue);
                break;
            case 'text':
                $size = isset($s['size']) ? $s['size'] : 32;
                $maxlength = isset($s['maxlength']) ? $s['maxlength'] : 32;
                $defaultValue = isset($s['defaultValue']) ? $s['defaultValue'] : "";

                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, '', $defaultValue, $callback);
                break;
            case 'password':
                $size = isset($s['size']) ? $s['size'] : 32;
                $maxlength = isset($s['maxlength']) ? $s['maxlength'] : 32;
                $defaultValue = isset($s['defaultValue']) ? $s['defaultValue'] : "";

            	PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, '', $defaultValue, $callback, '', "password");
                break;
            case 'number':
                $min = isset($s['min']) ? $s['min'] : 0;
                $max = isset($s['max']) ? $s['max'] : 99;
                $step = isset($s['step']) ? $s['step'] : 1;
                $unit = isset($s['unit']) ? $s['unit'] : '';
                $defaultValue = isset($s['defaultValue']) ? $s['defaultValue'] : "0";

                PrintSettingTextSaved($setting, $restart, $reboot, $max, $min, '', $defaultValue, $callback, '', 'number');
                echo $unit . ' ';
                break;
            default:
                printf( "FIXME, handle %s setting type for %s\n", $s['type'], $setting);
                break;
        }

        echo "<img id='$setting" . "_img' title='$setting' src='images/questionmark.png'><span id='$setting" . "_tip' class='tooltip' style='display: none'>" . $s['tip'] . "</span>\n";

        if ($level == 1)
            echo " <b>*</b>";
        else if ($level == 2)
            echo " <b>**</b>";
        else if ($level == 3)
            echo " <b>***</b>";

        echo "</td></tr>\n";
    }
}

function printSettingGroup($group) {
    global $settings;
    global $settingGroups;

    LoadSettingInfos();

    if (!isset($settingGroups[$group])) {
    echo "<!--\n";
    var_dump($settingGroups);
    echo "-->\n";
        echo "<b>ERROR: Invalid Setting Group: $group</b><br>\n";
        return;
    }

    $g = $settingGroups[$group];
    $level = isset($g['level']) ? $g['level'] : 0;

    if (($settings['uiLevel'] >= $level) &&
        ((!isset($g['fppModes'])) ||
         (in_array('ALL', $g['fppModes'])) ||
         (in_array($settings['fppMode'], $g['fppModes']))) &&
        ((!isset($g['platforms'])) ||
         (in_array('ALL', $g['platforms'])) ||
         (in_array($settings['Platform'], $g['platforms'])))) {
        echo "<b>" . $g['desc'] . "</b>\n";
        echo "<table class='settingsTable settingsGroupTable'>\n";

        foreach ($g['settings'] as $setting) {
            PrintSetting($setting);
        }

        echo "</table><br>\n";
    }
}

function PrintSettingCheckbox($title, $setting, $restart = 1, $reboot = 0, $checkedValue, $uncheckedValue, $pluginName = "", $callbackName = "", $defaultValue = 0, $desc = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

    $escSetting = preg_replace('/\./', '\\\\\\\\.', $setting);

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

    $changedFunction = preg_replace('/\./', '', $setting . "Changed");

	echo "
<script>
function " . $changedFunction . "() {
	var value = '$uncheckedValue';
	var checked = 0;
	if ($('#$escSetting').is(':checked')) {
		checked = 1;
		value = '$checkedValue';
	}

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			if (checked)
				$.jGrowl('$title Enabled');
			else
				$.jGrowl('$title Disabled');
			$settingsName" . "['$setting'] = value;
";

if ($restart)
	echo "SetRestartFlag($restart);\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
			$callbackName
			CheckRestartRebootFlags();
		}).fail(function() {
			if (checked) {
				DialogError('$title', 'Failed to Enable $title');
				$('#$escSetting').prop('checked', false);
			} else {
				DialogError('$title', 'Failed to Disable $title');
				$('#$escSetting').prop('checked', true);
			}
		});
}
</script>

<input type='checkbox' id='$setting' ";

	IfSettingEqualPrint($setting, $checkedValue, "checked", $pluginName, $defaultValue);

	echo " onChange='" . $changedFunction . "();'";

    if ($desc != "")
        echo '>' . $desc . "</input>\n";
    else
        echo " />\n";
}

function PrintSettingSelect($title, $setting, $restart = 1, $reboot = 0, $defaultValue, $values, $pluginName = "", $callbackName = "", $changedFunction = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

    $escSetting = preg_replace('/\./', '\\\\\\\\.', $setting);

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

    if ($changedFunction == "")
        $changedFunction = preg_replace('/\./', '', $setting . "Changed");

	echo "
<script>
function " . $changedFunction . "() {
	var value = $('#$escSetting').val();

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			$.jGrowl('$title saved');
			$settingsName" . "['$setting'] = value;
			$callbackName
";

if ($restart)
	echo "SetRestartFlag($restart);\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
		}).fail(function() {
			DialogError('$title', 'Failed to save $title');
		});
}
</script>

<select id='$setting' onChange='" . $changedFunction . "();'>\n";

	foreach ( $values as $key => $value )
	{
		echo "<option value='$value'";

		
		if (isset($pluginSettings[$setting]) || isset($settings[$setting]))
			IfSettingEqualPrint($setting, $value, " selected", $pluginName);
        else if ($value == $defaultValue)
            echo " selected";
		echo ">$key</option>\n";
	}

	echo "</select>\n";
}

function PrintSettingText($setting, $restart = 1, $reboot = 0, $maxlength = 32, $size = 32, $pluginName = "", $defaultValue = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	echo "
<input type='text' id='$setting' maxlength='$maxlength' size='$size' value=\"";

	if (isset($settings[$setting]))
		echo $settings[$setting];
	elseif (isset($pluginSettings[$setting]))
		echo $pluginSettings[$setting];
	else
		echo $defaultValue;

	echo "\">\n";
}

function PrintSettingTextSaved($setting, $restart = 1, $reboot = 0, $maxlength = 32, $size = 32, $pluginName = "", $defaultValue = "", $callbackName = "", $changedFunction = "", $inputType = "text")
{ 
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

    $escSetting = preg_replace('/\./', '\\\\\\\\.', $setting);

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

    
    if ($callbackName != "")
        $callbackName = $callbackName . "();";
    if ($changedFunction == "")
        $changedFunction = preg_replace('/\./', '', $setting . "Changed");

    $maxTag = 'maxlength';
    $sizeTag = 'size';
    if ($inputType == 'number') {
        $maxTag = 'max';
        $sizeTag = 'min';
    }

    echo "
    <script>
    function " . $changedFunction . "() {
        var value = $('#$escSetting').val();
        $.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
        .done(function() {
              $.jGrowl('$setting Saved');
              $settingsName" . "['$setting'] = value;
              ";
              
              if ($restart)
                echo "SetRestartFlag($restart);\n";
              if ($reboot)
                echo "SetRebootFlag();\n";
              
              echo "
              $callbackName
              CheckRestartRebootFlags();
              }).fail(function() {
                      DialogError('$setting', 'Failed to save $setting');
              });
    }
    </script>

    
    <input type='$inputType' id='$setting' $maxTag='$maxlength' $sizeTag='$size' onChange='" . $changedFunction . "();' value=\"";

	if (isset($settings[$setting]))
		echo $settings[$setting];
	elseif (isset($pluginSettings[$setting]))
		echo $pluginSettings[$setting];
	else
		echo $defaultValue;

	echo "\">\n";
}
function PrintSettingPasswordSaved($setting, $restart = 1, $reboot = 0, $maxlength = 32, $size = 32, $pluginName = "", $defaultValue = "", $callbackName = "", $changedFunction = "")
{
	PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $pluginName, $defaultValue, $callbackName, $changedFunction, "password");
}


function PrintSettingSave($title, $setting, $restart = 1, $reboot = 0, $pluginName = "", $callbackName = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

    $escSetting = preg_replace('/\./', '\\\\\\\\.', $setting);

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

    $saveFunction = preg_replace('/\./', '', $setting . "Changed");

	echo "
<script>
function " . $saveFunction . "() {
	var value = $('#$escSetting').val();

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			$.jGrowl('$title saved');
			$settingsName" . "['$setting'] = value;
			$callbackName
";

if ($restart)
	echo "SetRestartFlag($restart);\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
		}).fail(function() {
			DialogError('$title', 'Failed to save $title');
			$('#$escSetting').prop('checked', false);
		});
}
</script>

<input type='button' class='buttons' id='save$setting' onClick='" . $saveFunction . "();' value='Save'>\n";
}

/**
 * Returns sequence header info for the specified sequence
 *
 * @param $mediaName
 * @return array
 */
function get_sequence_file_info($mediaName){
	global $settings;

	$total_duration=0;
	$media_filesize = 0;
	$filename = $settings['sequenceDirectory'] . "/" . $mediaName;

	$all_data = array(
		'seqFormatID' => null,
		'seqChanDataOffset' => null,
		'seqMinorVersion' => null,
		'seqMajorVersion' => null,
		'seqVersion' => null,
		'seqFixedHeaderSize' => null,
		'seqStepSize' => null,
		'seqNumPeriods' => null,
		'seqStepTime' => null,
		'seqNumUniverses' => null,
		'seqUniverseSize' => null,
		'seqGamma' => null,
		'seqColorEncoding' => null,
		'seqDuration' => null,
		'seqMSRemaining' => null,
		'seqMediaName' => null
	);
	//Make sure it exists first
	if (!empty($mediaName) & file_exists($filename)) {
		//Get the filesize
		$media_filesize = filesize($filename);
		//Read the sequence
		$file_handle = fopen($filename, "r");
		//Read the first 28 bytes for the header
		while (($data = fread($file_handle, 28)) !== FALSE && strlen($data) == 28) {

			//Break data down and split out the bits, ignore my horrible decoding method lol
			$all_data['seqFormatID'] = unpack("A*", substr($data, 0, 4))[1]; //uint32 - magic cookie -- Results in FSEQ
			$all_data['seqChanDataOffset'] = unpack("S*", substr($data, 4, 2))[1]; //uint32 - DataOffset -- only 2 bytes so really uint16 -- checks out on test sequence (1040) & FPPD log output
			$all_data['seqMinorVersion'] = unpack("C*", substr($data, 6, 1))[1]; //uint8 - Minor Version -- May not be correct decoding - results in 0
			$all_data['seqMajorVersion'] = unpack("C*", substr($data, 7, 1))[1]; //uint8 - Major version -- -- May not be correct decoding - results in 1... Version 1.0? seems correct
			//build version
			$all_data['seqVersion'] = $all_data['seqMajorVersion'] . "." . $all_data['seqMinorVersion'];

			$all_data['seqFixedHeaderSize'] = unpack("S*", substr($data, 8, 2))[1];  //uint16 - FixedHeader: 28 - checks out with FPPD log output
			$all_data['seqStepSize'] = unpack("L*", substr($data, 10, 4))[1]; //uint32 - StepSize
			$all_data['seqNumPeriods'] = unpack("L*", substr($data, 14, 4))[1]; //uint32 - NumSteps
			$all_data['seqStepTime'] = unpack("S*", substr($data, 18, 2))[1]; //uint16 - StepTime (eg 50ms)
			$all_data['seqNumUniverses'] = unpack("S*", substr($data, 20, 2))[1]; //uint16 - Number of Universes -- Def: 0 //Ignored by Pi Player
			$all_data['seqUniverseSize'] = unpack("S*", substr($data, 22, 2))[1]; //uint16 - Size of Universes -- Def: 0 //Ignored by Pi Player
			$all_data['seqGamma'] = unpack("C*", substr($data, 24, 1))[1]; //uint8 - Gamma -- Def: 1?
			$all_data['seqColorEncoding'] = unpack("C*", substr($data, 25, 1))[1]; //uint8 - Type  -- Def: 2? -- RGB Channels //0=encoded, 1=linear, 2=RGB
//			$all_data['seqUnusedField'] = unpack("S*", substr($data, 26, 2))[1]; //uint16 - unused

			//VARIABLE HEADER
			//to read the variable header, seek to offset 28, then read data between 28 and seqChanDataOffset..
			fseek($file_handle, 28);
			//only read first 128 bytes just to catch everything, shouldn't need more than this for our purpose
			$seek_data = fread($file_handle, 128); //$all_data['seqChanDataOffset'] - 28
			//make sure we have data
			if ($seek_data !== FALSE && strlen($seek_data) == 128) {
				//Variable header structures start with:
				//uint16_t: length of structure (including self)
				//uint16_t: type code for structure
				//(Other data in structure)

				//Get the length and type of code
				$variable_header_len = unpack("S*", substr($seek_data, 0, 2))[1]; //uint16 - header len decodes correct
				//Vixen 3 and xLights have the next 16 bits identified as 'mf' presumably for 'media  filename'
				$variable_header_type_of_code = unpack("A*", substr($seek_data, 2, 2))[1]; //uint16 - unsure if decodes correct - treat as string

				//Filename offset starts at 5(Vixen) and 4(xLights) after the header stuff,
				//this is set correctly with a dirty fix around Vixen
				$media_filename_data_offset = 5;
				//is mf or media file then continue else fail so we don't read a non media file variable header
				if (!empty($variable_header_type_of_code) && ($variable_header_type_of_code == "mf" || strtolower($variable_header_type_of_code) == "mf")) {
					//In Vixen FSEQ files there is there length value preceding the actual media filename
					//it's easy to forward the offset by one and this works except there's no wait to tell if we're reading a
					$fn_length_ident = unpack("C*", substr($seek_data, 4, 1))[1];

					//Get the filename
					//media filename should start at offset 4, in vixen it's offset 5 due to the length value in offset 4 being the filename length
					//Length of the substring is the header len - the offset... just so we don't read into the next bit or outside the header
					$remaining_header_len = ($variable_header_len - $media_filename_data_offset);
					//For Vixen this is just the media filename
					//For xLights this is the full path to the media at the time of export
					$sequenceMediaName = unpack("A*", substr($seek_data, $media_filename_data_offset, $remaining_header_len))[1]; //decode as string

					//dirty fix for Vixen FSEQ files
					//In vixen the length of filename string is at offset 4 (then the filename follows)
					//because we can't tell if the sequence is from Vixen, check the value in offset 4
					//if we're processing a Vixen sequence then it will match the length of the filename (starting at offset 5)
					if (strlen($sequenceMediaName) == $fn_length_ident) {
						//Vixen - do nothing, if we end up here it's likely that we're reading a FSEQ from Vixen
					} else {
						//xLights, then move the offset back to 4 (since there is no filename length and we just read the whole lot)
						//for xLights file names it's the entire path to the media file, so it's going to be unlikely that the integer value of the first character "C" (C:\ - Windows) or "/" (Linux or Mac)
						//will match the length of the filename so we're safe in stepping back the offset
						$media_filename_data_offset = 4;
						$remaining_header_len = ($variable_header_len - $media_filename_data_offset);
						$sequenceMediaName = unpack("A*", substr($seek_data, $media_filename_data_offset, $remaining_header_len))[1]; //decode as string
					}

//					$all_data['variable_header_len'] = $variable_header_len;
//					$all_data['variable_header_type_of_code'] = $variable_header_type_of_code;
//					$all_data['variable_header_media_filename'] = unpack("A*", substr($seek_data, $media_filename_data_offset, $remaining_header_len));
//					$all_data['variable_header_media_offset'] = $media_filename_data_offset;
//					$all_data['variable_header_filename_len'] = $gs_ident;
//					$all_data['variable_header_filename_actual_len'] = strlen($sequenceMediaName);

					//Fix up the string by replacing escaped backward slash
					//then check to see if we can breakdown the string for path info
					$sequenceMediaName_pathinfo = pathinfo(str_replace("\\", "/", $sequenceMediaName));

					//Get Basename (final filename)
					if (array_key_exists('basename', $sequenceMediaName_pathinfo) && !empty($sequenceMediaName_pathinfo['basename'])) {
						//For some reason some xLights sequences
						$all_data['seqMediaName'] = $sequenceMediaName_pathinfo['basename'];
					} else {
						$all_data['seqMediaName'] = "";
					}
				} else {
					error_log("get_sequence_file_info:: Unable to read sequence variable header :: " . $mediaName);
				}
			} else {
				error_log("get_sequence_file_info:: Unable to seek to offset 28 for variable header :: " . $mediaName);
			}

			//Workout the duration
			//Time duration: ((NumSteps) * (StepTime) / 1000) seconds
			$all_data['seqDuration'] = (($all_data['seqNumPeriods']) * ($all_data['seqStepTime']) / 1000);
			$all_data['seqMSRemaining'] = (($all_data['seqNumPeriods']) * ($all_data['seqStepTime']));
			$all_data['seqFileSize'] = $media_filesize; //filesize

			//Break the loop, because we don't want to process any further
			break;
		}
		if ($file_handle === FALSE) {
			error_log("get_sequence_file_info:: Unable to read sequence info for :: " . $mediaName);
		}
		fclose($file_handle);
		unset($data);
		unset($seek_data);
	} else {
		error_log("get_sequence_file_info:: Cannot find sequence :: " . $mediaName);
	}

	return $all_data;
}

/**
 * Retrieving the duration of media files takes roughly 100+ms on a Pi2, over large playlists the delays can add up
 * to speed things up we'll try cache the durations so we don't unnecessarily keep hitting the media files
 *
 * If the the media duration exists in the cache then it's returned, else the returned value is null
 *
 * @param $media
 * @param null $duration_seconds
 * @return null
 */
function media_duration_cache($media, $duration_seconds = null, $filesize = null)
{
	global $settings;
	$config_dir = $settings['configDirectory'];
	$cache_file = "media_durations.cache";
	$file_path = $config_dir . "/" .$cache_file ;
	$time = 86400 * 30; //seconds - cache for 24hrs * 30days
	$duration_cache = array();
	$return_duration = null;

	if (!file_exists($file_path) ||  ( time() - filemtime( $file_path ) > $time)) {
		// cache doesn't exist or expired so lets create it and insert our media entry
		if ($duration_seconds !== null) {
			//put the media duration into the cache, but only if it isn't null
			$duration_cache[$media] = array('filesize' => $filesize, 'duration' => $duration_seconds);

			$return_duration = $duration_seconds;
			file_put_contents($file_path, json_encode($duration_cache, JSON_PRETTY_PRINT));
		}
	} else {
		//else cache exists and is valid, replaces/append duration to it
		$duration_cache = file_get_contents($file_path);
		if ($duration_cache !== FALSE && !empty($duration_cache)) {
			$duration_cache = json_decode($duration_cache, true);
			//if file hashes are the same - then it's the same file
			if (array_key_exists($media, $duration_cache) && $duration_cache[$media]['filesize'] == $filesize) {
				//Key exists, then return the cached duration
				$return_duration = $duration_cache[$media]['duration'];
			} else if ($duration_seconds !== null) {
				//put the media duration into the cache, but only if it isn't null
				$duration_cache[$media] = array('filesize' => $filesize, 'duration' => $duration_seconds);;
				$return_duration = $duration_seconds;

				file_put_contents($file_path, json_encode($duration_cache, JSON_PRETTY_PRINT));
			}
		}
	}

	return $return_duration;
}

/**
 * Returns the supplied filesize in bytes in a human readable format
 * @param $bytes
 * @param int $decimals
 * @return string
 */
function human_filesize($path) {
    // cannot use filesize($path) as that returns a signed 32bit number so maxes out at 2GB
    $kbytes = trim(shell_exec("du -k \"" . $path . "\" | cut -f1 "));
    if (strlen($kbytes) < 3) {
        $bytes = filesize($path);
        $sz = 'BKMGTP';
        $factor = floor((strlen($bytes) - 1) / 3);
		if ($factor)
	        return sprintf("%.2f", $bytes / pow(1024, $factor)) . @$sz[$factor] . ($factor > 0 ?  "B" : "");
        return sprintf("%d", $bytes / pow(1024, $factor)) . @$sz[$factor] . ($factor > 0 ?  "B" : "");
    }
	$sz = 'KMGTP';
	$factor = floor((strlen($kbytes) - 1) / 3);
	return sprintf("%.2f", $kbytes / pow(1024, $factor)) . @$sz[$factor] . "B";
}

/**
 * Returns the supplied duration in a human readable format (eg. 03m:55s)
 * @param $total_duration
 * @return string
 */
function human_playtime($total_duration){
	$hour_return = "";
	//So we can leave the hours out if it duration isn't long enough
	if (floor($total_duration / 3600) > 0) {
		$hour_return = sprintf("%'02d", floor($total_duration / 3600)) . "h:";
	}

	return $hour_return . sprintf("%'02d", floor($total_duration / 60) % 60) . "m:" . sprintf("%'02d", $total_duration % 60) . "s";
}

/**
 * Returns current memory usage
 * @return float|int
 */
function get_server_memory_usage(){
	$fh = fopen('/proc/meminfo','r');
	$total = 0;
	$free = 0;
	$buffers = 0;
	$cached = 0;
	while ($line = fgets($fh)) {
		$pieces = array();
		if (preg_match('/^MemTotal:\s+(\d+)\skB$/', $line, $pieces)) {
			$total = $pieces[1];
		} else if (preg_match('/^MemFree:\s+(\d+)\skB$/', $line, $pieces)) {
			$free = $pieces[1];
		} else if (preg_match('/^Buffers:\s+(\d+)\skB$/', $line, $pieces)) {
			$buffers = $pieces[1];
		} else if (preg_match('/^Cached:\s+(\d+)\skB$/', $line, $pieces)) {
			$cached = $pieces[1];
		}
	}
	fclose($fh);

	$used = $total - $free - $buffers - $cached;
	$memory_usage = 1.0 * $used / $total * 100;

	return $memory_usage;
}

/**
 * Simple cache for storing data for a specified time interval
 *
 * @param $cache_name
 * @param $data_to_cache
 * @param int $cache_age
 * @return mixed|string
 */
function file_cache($cache_name, $data_to_cache, $cache_age = 90)
{
	$file_path = "/tmp/cache_" . $cache_name . ".cache";
	$cache_time = $cache_age; //seconds

	$cache_data_return = null;
	$cache_data = array();

	if (!file_exists($file_path) || (time() - filemtime($file_path) > $cache_time)) {
		// cache doesn't exist or expired so lets create it and insert our data
		if ($data_to_cache !== null || $data_to_cache !== "") {
			//put the supplied data into the cache, but only if it isn't null
//			$cache_data[$cache_name] = array('data' => $data_to_cache);
			$cache_data_return = $data_to_cache;

//			exec("echo \"$data_to_cache\" | sudo tee $file_path", $output, $return_val);
			file_put_contents($file_path, $data_to_cache);
		}
	} else {
		//else cache exists and is valid, replaces/append duration to it
		$cache_data_contents = file_get_contents($file_path);

//		$handle = fopen($file_path, 'r');
//		$cache_data_contents = trim(fread($handle,filesize($file_path)));
//		fclose($handle);

		if ($cache_data_contents !== FALSE && !empty($cache_data_contents)) {
//			$cache_data = json_decode($cache_data, true);
			//if file hashes are the same - then it's the same file
			//if (array_key_exists($cache_name, $cache_data)) {
			//Key exists, then return the cached duration
//			$cache_data_return = $cache_data[$cache_name]['data'];
			$cache_data_return = $cache_data_contents;
		} else if ($data_to_cache !== null || $data_to_cache !== "") {
			//put the supplied data into the cache
//			$cache_data[$cache_name] = array('data' => $data_to_cache);
			$cache_data_return = $data_to_cache;

//			exec("echo \"$data_to_cache\" | sudo tee $file_path", $output, $return_val);
			file_put_contents($file_path, $data_to_cache);
		}
	}
	return $cache_data_return;
}

function get_kernel_version(){
	$kernel_version = "";
	$cachefile_name = "kernel_version";
	$cache_age = 86400;

	$cached_data = file_cache($cachefile_name, NULL, $cache_age);
	if ($cached_data == NULL) {
		$kernel_version = exec("uname -r");
		//cache result
		file_cache($cachefile_name, $kernel_version, $cache_age);
	} else {
		$kernel_version = $cached_data;
	}

	return $kernel_version;
}

    
function get_cpu_stats() {
    $stats = @file_get_contents("/proc/stat");
    if ($stats !== false) {
        // Remove double spaces to make it easier to extract values with explode()
        $stats = preg_replace("/[[:blank:]]+/", " ", $stats);

        // Separate lines
        $stats = str_replace(array("\r\n", "\n\r", "\r"), "\n", $stats);
        $stats = explode("\n", $stats);
        foreach ($stats as $statLine) {
            $statLineData = explode(" ", trim($statLine));
            // Found!
            if ((count($statLineData) >= 5)
                && ($statLineData[0] == "cpu")) {
                return array(
                    $statLineData[1],
                    $statLineData[2],
                    $statLineData[3],
                    $statLineData[4],
                    $statLineData[5],
                    $statLineData[6],
                    $statLineData[7],
                );
            }
        }
    }
    return array(0, 0, 0 ,0);
}
    
/**
 * Returns average CPU usage
 * @return mixed
 */
function get_server_cpu_usage(){
    if (!file_exists("/tmp/cpustats.txt")) {
        $ostats = get_cpu_stats();
        $vs = sprintf("%d %d %d %d %d %d %d", $ostats[0],$ostats[1], $ostats[2], $ostats[3], $ostats[4], $ostats[5], $ostats[6]);
        @file_put_contents("/tmp/cpustats.txtt", $vs);
        usleep(10000);
    } else {
        $statLine = @file_get_contents("/tmp/cpustats.txt");
        $ostats = explode(" ", trim($statLine));
    }
    $stats = get_cpu_stats();
    $vs = sprintf("%d %d %d %d %d %d %d", $stats[0], $stats[1], $stats[2], $stats[3], $stats[4], $stats[5], $stats[6]);
    @file_put_contents("/tmp/cpustats.txt", $vs);
    
    $user = $stats[0] - $ostats[0];
    $nice = $stats[1] - $ostats[1];
    $system = $stats[2] - $ostats[2];
    $idle = $stats[3] - $ostats[3];
    $iowait = $stats[4] - $ostats[4];
    $irq = $stats[5] - $ostats[5];
    $softirq = $stats[6] - $ostats[6];

    $total = $user + $nice + $system + $idle + $iowait + $irq + $softirq;
    $val = $idle * 100.0;
    // 100 - the percent idle
    $val = 100.0 - ($val / $total);
    
	return $val;
}

/**
 * Returns server uptime
 * @param bool $uptime_value_only
 * @return null|string|string[]
 */
function get_server_uptime($uptime_value_only=false){
	$uptime = exec("uptime", $output, $return_val);
	if ($return_val != 0)
		$uptime = "";
	unset($output);
	$uptime = preg_replace('/[0-9]+ users?, /', '', $uptime);
	if ($uptime_value_only) {
		$uptime_portion = explode(",", $uptime,2)[0];
		if (!empty($uptime_portion) && stripos($uptime_portion, "up") !== false) {
			$uptime = trim(explode("up", $uptime_portion, 2)[1]);
		}
	}
	return $uptime;
}

/**
 * Returns the FPP head version
 *
 * @return mixed|string
 */
function get_fpp_head_version(){
	$fpp_head_version = "Unknown";
	$cachefile_name = "git_fpp_head_version";
	$cache_age = 90;

	$cached_data = file_cache($cachefile_name, NULL, $cache_age);
	if ($cached_data == NULL) {
		$fpp_head_version = exec("git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ describe --tags", $output, $return_val);
		if ($return_val != 0)
			$fpp_head_version = "Unknown";
		unset($output);
		//cache result
		file_cache($cachefile_name, $fpp_head_version, $cache_age);
	} else {
		$fpp_head_version = $cached_data;
	}

	return $fpp_head_version;
}

/**
 * Returns the current system Git branch
 * @return string
 */
function get_git_branch(){
	$git_branch = "";
	$cachefile_name = "git_branch";
	$cache_age = 90;

	$cached_data = file_cache($cachefile_name,NULL,$cache_age);
	if ($cached_data == NULL) {
		$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
		if ( $return_val != 0 )
			$git_branch = "Unknown";
		unset($output);
		//cache result
		file_cache($cachefile_name, $git_branch, $cache_age);
	}else{
		$git_branch = $cached_data;
	}

	return $git_branch;
}

/**
 * Returns the version of the local Git branch
 * @return string
 */
function get_local_git_version(){
	$git_version = "Unknown";
	$cachefile_name = "local_git_version";
	$cache_age = 90;

	$cached_data = file_cache($cachefile_name,NULL,$cache_age);
	if ($cached_data == NULL) {
		$git_version = exec("git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ rev-parse --short=7 HEAD", $output, $return_val);
		if ( $return_val != 0 )
			$git_version = "Unknown";
		unset($output);
		//cache result
		file_cache($cachefile_name, $git_version, $cache_age);
	}else{
		$git_version = $cached_data;
	}

	return $git_version;
}

/**
 * Returns version of the remote Git branch for the supplied branch
 * @return string
 */
function get_remote_git_version($git_branch){
	$git_remote_version = "Unknown";


	if (!empty($git_branch) || strtolower($git_branch) != "unknown") {
		$cachefile_name = "git_" . $git_branch;
		$cache_age = 90;
		$git_remote_version = "Unknown";

		//Check the cache for git_<branch>, if null is returned no cache file exists or it's expired, so then off to github
		$cached_data = file_cache($cachefile_name, NULL, $cache_age);
		if ($cached_data == NULL) {
			//if for some reason name resolution fails ping will take roughly 10 seconds to return (Default DNS Timeout 5 seconds x 2 retries)
			//to try work around this ping the google public DNS @ 8.8.8.8 (to skip DNS) waiting for a reply for max 1 second, if that's ok we have a route to the internet, then it's highly likely DNS will also work
			$google_dns_ping = exec("ping -q -c 1 -W 1 8.8.8.8 > /dev/null", $output, $return_val);
			unset($output);
			if ($return_val == 0){
				//Google DNS Ping success
				// this can take a couple seconds to complete so we'll cache it
				$git_remote_version = exec("ping -q -c 1 github.com > /dev/null && (git --git-dir=/opt/fpp/.git/ ls-remote -q -h origin $git_branch | awk '$1 > 0 { print substr($1,1,7)}')", $output, $return_val);
				if ($return_val != 0)
					$git_remote_version = "Unknown";
				unset($output);
			}else{
				//Google DNS Ping fail - return unknown
				$git_remote_version = "Unknown";
			}

			//cache result
			file_cache($cachefile_name, $git_remote_version, $cache_age);
		} else {
			//return the cached version
			$git_remote_version = $cached_data;
		}

//        if( ! file_exists( $file )  ||  ( time() - filemtime( $file ) > $time)) {
//            // this can take a couple seconds to complete so we'll cache it
//            $ver = exec("ping -q -c 1 github.com > /dev/null && (git --git-dir=/opt/fpp/.git/ ls-remote -q -h origin $git_branch | awk '$1 > 0 { print substr($1,1,7)}')", $output, $return_val);
//            if ( $return_val != 0 )
//                $ver = "Unknown";
//            unset($output);
//
//            exec("echo \"$ver\" | sudo tee $file", $output, $return_val);
//            unset($output);
//        } else {
//            $handle = fopen($file, 'r');
//            $ver = trim(fread($handle,filesize($file)));
//            fclose($handle);
//        }
	}

	return $git_remote_version;
}

function ReplaceIllegalCharacters($input_string)
{
	// Removes any of the following characters from the supplied name, can be used to cleanse playlist names, event names etc
	// Current needed for example it the case of the scheduler since it is still CSV and commas in a playlist name cause issues
	// Everything is currently replaced with a hyphen ( - )

	// , (comma)
	// < (less than)
	// > (greater than)
	// : (colon)
	// " (double quote)
	// / (forward slash)
	// \ (backslash)
	// | (vertical bar or pipe)
	// ? (question mark)
	// * (asterisk)

	$illegalChars = [',', '<', '>', ':', '"', '/', '\\', '|', '?', '*'];

	if (!empty($input_string)) {

		for ($ill_index = 0; $ill_index < count($illegalChars); $ill_index++) {
			$input_string = str_replace($illegalChars[$ill_index], '-', $input_string);
		}

	}

	return $input_string;
}

/**
 * Restarts the ntp server service
 */
function NtpServiceRestart(){
	global $SUDO;

	exec($SUDO . " service ntp restart", $output, $return_val);
	unset($output);
	//TODO: check return
}

/**
 * Sets the NTP server source to the supplied NTP server, if nothing is supplied default to the debian pool
 * @param $ntp_server String DNS or IP address of a NTP server
 */
function SetNtpServer($ntp_server){
	global $SUDO;

	WriteSettingToFile("ntpServer",$ntp_server);
	$settings['ntpServer'] = $ntp_server;
	if ($ntp_server != "")
	{
		exec($SUDO . " sed -i '/^server.*/d' /etc/ntp.conf ; " . $SUDO . " sed -i '\$s/\$/\\nserver " . $ntp_server . " iburst/' /etc/ntp.conf");
	}
	else
	{
		exec($SUDO . " sed -i '/^server.*/d' /etc/ntp.conf ; " . $SUDO . " sed -i '\$s/\$/\\nserver 0.debian.pool.ntp.org iburst\\nserver 1.debian.pool.ntp.org iburst\\nserver 2.debian.pool.ntp.org iburst\\nserver 3.debian.pool.ntp.org iburst\\n/' /etc/ntp.conf");
	}
}

/**
 * Toggles NTP service state
 * @param $state int 1 to Enable, 0 to Disable
 */
function SetNtpState($state){
	global $SUDO;

    WriteSettingToFile("NTP",$state);

    if($state == true){
        error_log("Enabling NTP because it's disabled and we were told to enable it.");
        exec($SUDO . " update-rc.d ntp defaults", $output, $return_val);
        unset($output);
        //TODO: check return
        exec($SUDO . " service ntp start", $output, $return_val);
        unset($output);
        //TODO: check return
    }else if ($state == false){
        error_log("Disabling NTP because it's enabled and we were told to disable it.");
        exec($SUDO . " service ntp stop", $output, $return_val);
        unset($output);
        //TODO: check return
        exec($SUDO . " update-rc.d ntp remove", $output, $return_val);
        unset($output);
        //TODO: check return
    }
}

/**
 * Generates appropriate files and settings for exim4
 * @param $emailguser String Gmail Username
 * @param $emailgpass String Gmail Password
 * @param $emailfromtext String Mail From address (eg. fpp01@example.com)
 * @param $emailtoemail String Destination email address
 */
function SaveEmailConfig($emailguser, $emailgpass, $emailfromtext, $emailtoemail){
    global $exim4Directory;

    $fp = fopen($exim4Directory . '/passwd.client', 'w');
    fwrite($fp, "# password file used when the local exim is authenticating to a remote host as a client.\n");
    fwrite($fp, "#\n");
    fwrite($fp, "*.google.com:" . $emailguser . ":" . $emailgpass . "\n");
    fwrite($fp, "smtp.gmail.com:" . $emailguser . ":" . $emailgpass . "\n");
    fclose($fp);
    exec("sudo cp " . $exim4Directory . "/passwd.client /etc/exim4/");
    exec("sudo update-exim4.conf");
    exec ("sudo /etc/init.d/exim4 restart");
    exec ("sudo systemctl restart exim4.service");
    $cmd="sudo chfn -f \"" . $emailfromtext . "\" fpp";
    exec($cmd);
    $fp = fopen($exim4Directory . '/aliases', 'w');
    fwrite($fp, "mailer-daemon: postmaster\npostmaster: root\nnobody: root\nhostmaster: root\nusenet: root\nnews: root\nwebmaster: root\nwww: root\nftp: root\nabuse: root\nnoc: root\nsecurity: root\nroot: pi\n");
    fwrite($fp, "pi: " . $emailtoemail . "\n");
    fclose($fp);
    exec("sudo cp " . $exim4Directory . "/aliases /etc/");
}


// This function is from:
// http://stackoverflow.com/questions/6054033/pretty-printing-json-with-php
function prettyPrintJSON( $json )
{
	$result = '';
	$level = 0;
	$in_quotes = false;
	$in_escape = false;
	$ends_line_level = NULL;
	$json_length = strlen( $json );

	for( $i = 0; $i < $json_length; $i++ ) {
		$char = $json[$i];
		$new_line_level = NULL;
		$post = "";
		if( $ends_line_level !== NULL ) {
			$new_line_level = $ends_line_level;
			$ends_line_level = NULL;
		}
		if ( $in_escape ) {
			$in_escape = false;
		} else if( $char === '"' ) {
			$in_quotes = !$in_quotes;
		} else if( ! $in_quotes ) {
			switch( $char ) {
				case '}': case ']':
					$level--;
					$ends_line_level = NULL;
					$new_line_level = $level;
					break;

				case '{': case '[':
					$level++;
				case ',':
					$ends_line_level = $level;
					break;

				case ':':
					$post = " ";
					break;

				case " ": case "\t": case "\n": case "\r":
					$char = "";
					$ends_line_level = $new_line_level;
					$new_line_level = NULL;
					break;
			}
		} else if ( $char === '\\' ) {
			$in_escape = true;
		}
		if( $new_line_level !== NULL ) {
			$result .= "\n".str_repeat( "\t", $new_line_level );
		}
		$result .= $char.$post;
	}

	return $result;
}

function DisableOutputBuffering() {
	// for NGINX, set the X-Accel-Buffering header
	header('X-Accel-Buffering: no');

	// Turn off output buffering
	ini_set('output_buffering', 'off');

	// Turn off PHP output compression
	ini_set('zlib.output_compression', false);

	// Implicitly flush the buffer(s)
	ini_set('implicit_flush', true);
	ob_implicit_flush(true);

	// Clear, and turn off output buffering
	while (ob_get_level() > 0) {
		// Get the curent level
		$level = ob_get_level();
		// End the buffering
		ob_end_clean();
		// If the current level has not changed, abort
		if (ob_get_level() == $level) break;
	}

	// Disable apache output buffering/compression
	if (function_exists('apache_setenv')) {
		apache_setenv('no-gzip', '1');
		apache_setenv('dont-vary', '1');
	}

	ob_implicit_flush(true);
	flush();
}

function ToolTip($setting, $tip) {
    echo "<img id='$setting" . "_img' title='$setting' src='images/questionmark.png'><span id='$setting" . "_tip' class='tooltip' style='display: none'>" . $tip . "</span>\n";
}

?>
