<?php 
require_once("config.php");

function ReadSettingFromFile($settingName)
{
	global $settingsFile;
	$settings = file_get_contents($settingsFile);
  if ( !empty($settings) )
  {
		if (preg_match("/^" . $settingName . "/m", $settings))
		{
      $result = preg_match("/^" . $settingName . "\s*=(\s*\S*\w*)/m", $settings, $output_array);
      if($result == 0)
      {
        error_log("The setting " . $settingName . " could not be found in " . $settingsFile);
        return false;
      }
      return trim($output_array[1]);
		}
		else
		{
      error_log("The setting " . $settingName . " could not be found in " . $settingsFile);
			return false;
		}
  }
	else
	{
    error_log("The setting file:" . $settingsFile . " could not be found." );
		return false;
	}
}

function WriteSettingToFile($settingName, $setting)
{
	global $settingsFile;
	$settings = file_get_contents($settingsFile);
	if ( !empty($settings) )
	{
		if (preg_match("/^" . $settingName . "\s*=/m", $settings))
		{
			$settings = preg_replace("/^" . $settingName . "\s*=\s*\S*\w*/m", $settingName . " = " . $setting . "\n", $settings);
		}
		else
		{
			$settings .= $settingName . " = " . $setting . "\n";
		}
		$settings = preg_replace("/\n\n/", "\n", $settings);
		file_put_contents($settingsFile, $settings);
	}
	else
	{
		file_put_contents($settingsFile, $settingName ." = " . $setting . "\n");
	}
}

function IfSettingEqualPrint($setting, $value, $print)
{
	global $settings;

	if ((isset($settings[$setting])) &&
		($settings[$setting] == $value ))
		echo $print;
}

function PrintSettingCheckbox($title, $setting, $checkedValue, $uncheckedValue)
{
	global $settings;

	echo "
<script>
function " . $setting . "Changed() {
	var value = '$uncheckedValue';
	var checked = 0;
	if ($('#$setting').is(':checked')) {
		checked = 1;
		value = '$checkedValue';
	}

	$.get('fppjson.php?command=setSetting&key=$setting&value=' + value)
		.success(function() {
			if (checked)
				$.jGrowl('$title Enabled');
			else
				$.jGrowl('$title Disabled');
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

	IfSettingEqualPrint($setting, $checkedValue, "checked");

	echo " onChange='" . $setting . "Changed();'>\n";
}
?>

