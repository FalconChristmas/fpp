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

function IfSettingEqualPrint($setting, $value, $print, $pluginName = "")
{
	global $settings;
	global $pluginSettings;

	if ($pluginName != "") {
		if ((isset($pluginSettings[$setting])) &&
			($pluginSettings[$setting] == $value ))
			echo $print;
	} else {
		if ((isset($settings[$setting])) &&
			($settings[$setting] == $value ))
			echo $print;
	}
}

function PrintSettingCheckbox($title, $setting, $restart = 1, $reboot = 0, $checkedValue, $uncheckedValue, $pluginName = "", $callbackName = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

	echo "
<script>
function " . $setting . "Changed() {
	var value = '$uncheckedValue';
	var checked = 0;
	if ($('#$setting').is(':checked')) {
		checked = 1;
		value = '$checkedValue';
	}

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.success(function() {
			if (checked)
				$.jGrowl('$title Enabled');
			else
				$.jGrowl('$title Disabled');
			$settingsName" . "['$setting'] = value;
";

if ($restart)
	echo "SetRestartFlag();\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
			$callbackName
			CheckRestartRebootFlags();
		}).fail(function() {
			if (checked) {
				DialogError('$title', 'Failed to Enable $title');
				$('#$setting').prop('checked', false);
			} else {
				DialogError('$title', 'Failed to Disable $title');
				$('#$setting').prop('checked', true);
			}
		});
}
</script>

<input type='checkbox' id='$setting' ";

	IfSettingEqualPrint($setting, $checkedValue, "checked", $pluginName);

	echo " onChange='" . $setting . "Changed();'>\n";
}

function PrintSettingSelect($title, $setting, $restart = 1, $reboot = 0, $defaultValue, $values, $pluginName = "", $callbackName = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

	echo "
<script>
function " . $setting . "Changed() {
	var value = $('#$setting').val();

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.success(function() {
			$.jGrowl('$title saved');
			$settingsName" . "['$setting'] = value;
			$callbackName
";

if ($restart)
	echo "SetRestartFlag();\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
		}).fail(function() {
			DialogError('$title', 'Failed to save $title');
		});
}
</script>

<select id='$setting' onChange='" . $setting . "Changed();'>\n";

	foreach ( $values as $key => $value )
	{
		echo "<option value='$value'";

		if ($value == $defaultValue)
			echo " selected";
		else if (isset($pluginSettings[$setting]) || isset($settings[$setting]))
			IfSettingEqualPrint($setting, $value, " selected", $pluginName);

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

function PrintSettingSave($title, $setting, $restart = 1, $reboot = 0, $pluginName = "", $callbackName = "")
{
	global $settings;
	global $pluginSettings;

	$plugin = "";
	$settingsName = "settings";

	if ($pluginName != "") {
		$plugin = "Plugin";
		$settingsName = "pluginSettings";
	}

	if ($callbackName != "")
		$callbackName = $callbackName . "();";

	echo "
<script>
function save" . $setting . "() {
	var value = $('#$setting').val();

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.success(function() {
			$.jGrowl('$title saved');
			$settingsName" . "['$setting'] = value;
			$callbackName
";

if ($restart)
	echo "SetRestartFlag();\n";
if ($reboot)
	echo "SetRebootFlag();\n";

echo "
		}).fail(function() {
			DialogError('$title', 'Failed to save $title');
			$('#$setting').prop('checked', false);
		});
}
</script>

<input type='button' class='buttons' id='save$setting' onClick='save" . $setting . "();' value='Save'>\n";
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
    exec("sudo cp " . $exim4Directory . "./aliases /etc/");
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

?>
