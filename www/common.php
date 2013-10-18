<?php 
function ReadSettingFromFile($settingName)
{
	global $settingsFile;
	$settings = file_get_contents($settingsFile);
  if ( !empty($settings) )
  {
		if (!(strpos($settings, $settingName) === false))
		{
      $result = preg_match("/" . $settingName . "\s*=(\s*\S*\w*)/", $settings, $output_array);
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
		if (!(strpos($settings, $settingName) === false))
		{
			$settings = preg_replace("/" . $settingName . "\s*=\s*\S*\w*/", $settingName . " = " . $setting, $settings);
		}
		else
		{
			$settings .= "\n" . $settingName . " = " . $setting . "\n";
		}
		file_put_contents($settingsFile, $settings);
	}
	else
	{
		file_put_contents($settingsFile, $settingsFile ." = " . $setting . "\n");
	}
}

?>