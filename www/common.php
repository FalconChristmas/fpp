<?php
require_once "config.php";

function check($var, $var_name = "", $function_name = "")
{
    global $debug;

    if (empty($function_name)) {
        global $args;
        $function_name = $args['command'];
    }

    if (empty($var) && $debug) {
        error_log("WARNING: Variable '$var_name' in function '$function_name' was empty");
    }

}

function startsWith($haystack, $needle)
{
    $length = strlen($needle);
    return substr($haystack, 0, $length) === $needle;
}

function endsWith($haystack, $needle)
{
    $length = strlen($needle);
    if (!$length) {
        return true;
    }
    return substr($haystack, -$length) === $needle;
}

function getFileList($dir, $ext)
{
    $i = array();
    if ($handle = opendir($dir)) {
        while (($file = readdir($handle)) !== false) {
            if (!in_array($file, array('.', '..')) && !is_dir($dir . $file) && strtolower(substr($file, strrpos($file, '.') + 1)) == $ext) {
                array_push($i, $file);
            }
        }
    }
    return $i;
}

function ScrubFile($filename, $taboo = array("emailpass", "emailgpass", "MQTTPassword", "password", "passwordVerify", "osPassword", "osPasswordVerify"))
{
    if (!file_exists($filename)) {
        return "";
    }

    $dataStr = "";

    if (preg_match("/.json$/", $filename)) {
        // Need to scrub .json as well at some point
        $dataStr = file_get_contents($filename);
    } else {
        $data = [];
        $fd = @fopen($filename, "r");
        if ($fd) {
            flock($fd, LOCK_SH);
            $data = parse_ini_file($filename);
            flock($fd, LOCK_UN);
            fclose($fd);
        }

        foreach ($taboo as $key) {
            if (array_key_exists($key, $data)) {
                $data[$key] = "********";
            }
        }

        foreach ($data as $key => $value) {
            $dataStr .= $key . " = " . $value . "\n";
        }

    }

    return $dataStr;
}

function ReadSettingFromFile($settingName, $plugin = "")
{
    global $settingsFile;
    global $settings;
    $filename = $settingsFile;

    if ($plugin != "") {
        $filename = $settings["configDirectory"] . "/plugin." . $plugin;
    }
    if (!file_exists($filename)) {
        return false;
    }
    $fd = @fopen($filename, "r");
    $settingsStr = "";
    if ($fd) {
        flock($fd, LOCK_SH);
        $settingsStr = file_get_contents($filename);
        flock($fd, LOCK_UN);
        fclose($fd);
    }
    if (!empty($settingsStr)) {
        if (preg_match("/^" . $settingName . "/m", $settingsStr)) {
            $result = preg_match("/^" . $settingName . "\s*=(\s*\S*\w*)/m", $settingsStr, $output_array);
            if ($result == 0) {
//        error_log("The setting " . $settingName . " could not be found in " . $filename);
                return false;
            }
            return trim($output_array[1], " \t\n\r\0\x0B\"");
        } else {
//      error_log("The setting " . $settingName . " could not be found in " . $filename);
            return false;
        }
    } else {
        error_log("The setting file:" . $filename . " could not be found.");
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

    $fd = @fopen($filename, "c+");
    flock($fd, LOCK_EX);
    $tmpSettings = parse_ini_file($filename);
    if ($tmpSettings[$settingName] != $setting) {
        $tmpSettings[$settingName] = $setting;
        $settingsStr = "";
        foreach ($tmpSettings as $key => $value) {
            $settingsStr .= $key . " = \"" . $value . "\"\n";
        }
        file_put_contents($filename, $settingsStr);
    }
    flock($fd, LOCK_UN);
    fclose($fd);
}

function DeleteSettingFromFile($settingName, $plugin = "")
{
    global $settingsFile;
    global $settings;
    $filename = $settingsFile;

    if ($plugin != "") {
        $filename = $settings['configDirectory'] . "/plugin." . $plugin;
    }

    $fd = @fopen($filename, "c+");
    flock($fd, LOCK_EX);
    $tmpSettings = parse_ini_file($filename);
    if (isset($tmpSettings[$settingName])) {
        unset($tmpSettings[$settingName]);
        $settingsStr = "";
        foreach ($tmpSettings as $key => $value) {
            $settingsStr .= $key . " = \"" . $value . "\"\n";
        }
        file_put_contents($filename, $settingsStr);
    }
    flock($fd, LOCK_UN);
    fclose($fd);
}

function IfSettingEqualPrint($setting, $value, $print, $pluginName = "", $defaultValue = "")
{
    global $settings;
    global $pluginSettings;

    if ($pluginName != "") {
        if (($defaultValue != "") && !isset($pluginSettings[$setting])) {
            $pluginSettings[$setting] = $defaultValue;
        }

        if ((isset($pluginSettings[$setting])) &&
            ($pluginSettings[$setting] == $value)) {
            echo $print;
        }

    } else {
        if (($defaultValue != "") && !isset($settings[$setting])) {
            $settings[$setting] = $defaultValue;
        }

        if ((isset($settings[$setting])) &&
            ($settings[$setting] == $value)) {
            echo $print;
        }

    }
}

function IsInList($value, $list)
{
    $values = explode(",", $list);
    $ret = in_array($value, $values);
    return $ret;
}
function IfSettingInListPrint($setting, $value, $print, $pluginName = "", $defaultValue = "")
{
    global $settings;
    global $pluginSettings;

    if ($pluginName != "") {
        if (($defaultValue != "") && !isset($pluginSettings[$setting])) {
            $pluginSettings[$setting] = $defaultValue;
        }

        if ((isset($pluginSettings[$setting])) &&
            IsInList($value, $pluginSettings[$setting])) {
            echo $print;
        }

    } else {
        if (($defaultValue != "") && !isset($settings[$setting])) {
            $settings[$setting] = $defaultValue;
        }

        if ((isset($settings[$setting])) &&
            IsInList($value, $settings[$setting])) {
            echo $print;
        }

    }
}

function LoadPluginSettingInfos($plugin)
{
    global $settings;
    global $settingInfos;
    global $settingGroups;
    global $mediaDirectory;
    global $pluginSettingInfosLoaded;
    global $pluginSettingInfos;

    if (empty($settingInfos) || empty($settingGroups)) {
        LoadSettingInfos();
    }

    if (!$pluginSettingInfosLoaded) {
        $file = $mediaDirectory . '/plugins/' . $plugin . '/settings.json';

        if (file_exists($file)) {
            $data = json_decode(file_get_contents($file), true);
            $pluginSettingInfos = $data['settings'];
            $settingInfos = array_merge($settingInfos, $data['settings']);
            $settingGroups = array_merge($settingGroups, $data['settingGroups']);
        }
        $pluginSettingInfosLoaded = 1;
    }
}
function MergeDefaultsFromPluginSettings($plugin)
{
    global $pluginSettingInfos;
    global $pluginSettings;

    LoadPluginSettingInfos($plugin);
    foreach ($pluginSettingInfos as $key => $value) {
        if (!isset($pluginSettings[$key]) && isset($value["default"])) {
            $pluginSettings[$key] = $value["default"];
        }
    }
}
function LoadPluginSettings($pluginName)
{
    global $pluginSettings, $settings;
    $pluginConfigFile = $settings['configDirectory'] . "/plugin." . $pluginName;
    if (file_exists($pluginConfigFile)) {
        $pluginSettings = [];
        $fd = @fopen($pluginConfigFile, "r");
        if ($fd) {
            flock($fd, LOCK_SH);
            $pluginSettings = parse_ini_file($pluginConfigFile);
            flock($fd, LOCK_UN);
            fclose($fd);
        }
    }
    MergeDefaultsFromPluginSettings($pluginName);
}
function ParseBooleanValue($value)
{
    return $value == "1" || $value == "true" || $value == "TRUE" || $value == "ON" || $value == "on";
}

function PrintPluginSetting($plugin, $setting, $callback = '', $options = array())
{
    PrintSetting($setting, $callback, $options, $plugin);
}

function PrintIcon($level)
{
    if ($level == 1) {
        echo " <i class='fas fa-fw fa-graduation-cap fa-nbsp ui-level-1' title='Advanced Level Setting'></i>";
    } else if ($level == 2) {
        echo " <i class='fas fa-fw fa-flask fa-nbsp ui-level-2' title='Experimental Level Setting'></i>";
    } else if ($level == 3) {
        echo " <i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>";
    } else {
        echo " <i class='fas fa-fw fa-nbsp ui-level-0'></i>";
    }

}

function ShouldPrintSetting($s)
{
    global $settings;
    $level = isset($s['level']) ? $s['level'] : 0;

    if ($settings['uiLevel'] < $level) {
        return false;
    }
    if (isset($s['checkFile'])) {
        $checkFileOK = 0;
        foreach ($s['checkFile'] as $f) {
            $invertCheck = false;
            if ($f[0] == '!') {
                $f = substr($f, 1);
                $invertCheck = true;
            }
            if (file_exists($f)) {
                if (!$invertCheck) {
                    $checkFileOK = 1;
                }
            } else if ($invertCheck) {
                $checkFileOK = 1;
            }
        }
        if ($checkFileOK == 0) {
            return false;
        }
    }
    if (isset($s['fppModes'])) {
        if (!in_array('ALL', $s['fppModes']) &&
            !in_array($settings['fppMode'], $s['fppModes'])) {
            return false;
        }
    }
    if (isset($s['platforms'])) {
        $hasNonExclusion = false;
        $hasAll = false;
        $hasPlatform = false;
        foreach ($s['platforms'] as $platform) {
            if (("!" . $settings['Platform']) == $platform) {
                return false;
            }
            if ($platform[0] != "!") {
                $hasNonExclusion = true;
            }
            if ($platform == "ALL") {
                $hasAll = true;
            }
            if ($platform == $settings['Platform']) {
                $hasPlatform = true;
            }
        }
        if (!$hasAll && !$hasPlatform && $hasNonExclusion) {
            return false;
        }
    }
    if (isset($s['variants']) && isset($s['variants'][$settings['Platform']])) {
        $hasNonExclusion = false;
        $hasAll = false;
        $hasVariant = false;
        foreach ($s['variants'][$settings['Platform']] as $variant) {
            if (("!" . $settings['Variant']) == $variant) {
                return false;
            }
            if ($variant[0] != "!") {
                $hasNonExclusion = true;
            }
            if ($variant == "ALL") {
                $hasAll = true;
            }
            if ($variant == $settings['Variant']) {
                $hasVariant = true;
            }
        }
        if (!$hasAll && !$hasVariant && $hasNonExclusion) {
            return false;
        }
    }
    if (isset($s['settingValues'])) {
        foreach ($s['settingValues'] as $key => $value) {
            if (!isset($settings[$key]) || ($settings[$key] != $value)) {
                return false;
            }
        }
    }
    return true;
}

function PrintSetting($setting, $callback = '', $options = array(), $plugin = '')
{
    global $settings;
    global $settingInfos;
    global $pluginSettingInfos;

    LoadSettingInfos();

    if ($plugin != '') {
        LoadPluginSettingInfos($plugin);
    }

    if (!isset($settingInfos[$setting])) {
        echo "<div class='row'><div class='col td-colspan-2' colspan='2'><b>Invalid Setting: $setting</b></div></div>\n";
        return;
    }

    if ($plugin == '') {
        $s = $settingInfos[$setting];
    } else {
        $s = $pluginSettingInfos[$setting];
    }

    $level = isset($s['level']) ? $s['level'] : 0;
    $reloadUI = isset($s['reloadUI']) ? $s['reloadUI'] : 0;
    $textOnRight = isset($s['textOnRight']) ? $s['textOnRight'] : 0;

    if (($callback == '') && ($reloadUI == 1)) {
        $callback = 'reloadSettingsPage';
    }

    if (ShouldPrintSetting($s)) {
        $restart = isset($s['restart']) ? $s['restart'] : 0;
        $reboot = isset($s['reboot']) ? $s['reboot'] : 0;
        $suffix = isset($s['suffix']) ? $s['suffix'] : '';

        if ($textOnRight) {
            echo "<div class='row' id='" . $setting . "Row'><div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2'>";
        } else {
            echo "<div class='row' id='" . $setting . "Row'><div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2'><div class='description'>";
            PrintIcon($level);
            echo $s['description'] . "</div></div><div class='printSettingFieldCol col-md'>";
        }

        switch ($s['type']) {
            case 'datalist':
            case 'select':
                if (empty($options)) {
                    if (isset($s['optionsURL'])) {
                        $json = "";
                        $url = $s['optionsURL'];
                        if (!preg_match('/^\//', $s['optionsURL'])) {
                            $url = "http://127.0.0.1/" . $s['optionsURL'];
                        }
                        $json = file_get_contents($url);
                        $options = json_decode($json, true);
                        if (!(array_keys($options) !== range(0, count($options) - 1))) {
                            $tmp = $options;
                            $options = array();
                            if (isset($s['optionCanBeBlank']) && $s['optionCanBeBlank']) {
                                $options[""] = "";
                            }
                            foreach ($tmp as $item) {
                                if (isset($s['optionsKey'])) {
                                    $options[$item[$s['optionsKey']]] = $item[$s['optionsKey']];
                                } else {
                                    $options[$item] = $item;
                                }
                            }
                        } else if (isset($s['optionCanBeBlank']) && $s['optionCanBeBlank']) {
                            $options = array("" => "") + $options;
                        }
                    } else if (isset($s['optionsCommand'])) {
                        $optionsRaw = explode("\n", trim(shell_exec($s['optionsCommand'])));
                        foreach ($optionsRaw as $line) {
                            $parts = preg_split("/,/", trim($line));
                            if (count($parts) > 1) {
                                $options[$parts[0]] = $parts[1];
                            } else {
                                $options[$parts[0]] = $parts[0];
                            }

                        }
                    } else if (isset($s['options'])) {
                        $options = $s['options'];
                    }
                }

                $default = isset($s['default']) ? $s['default'] : "";

                if ($s['type'] == "select") {
                    PrintSettingSelect($s['description'], $setting, $restart, $reboot, $default, $options, $plugin, $callback, '', $s);
                } else {
                    $news = $s;
                    $news['options'] = $options;
                    $size = isset($s['size']) ? $s['size'] : 32;
                    $maxlength = isset($s['maxlength']) ? $s['maxlength'] : 32;
                    PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', 'datalist', $news);
                }
                break;
            case 'checkbox':
                $checkedValue = isset($s['checkedValue']) ? $s['checkedValue'] : "1";
                $uncheckedValue = isset($s['uncheckedValue']) ? $s['uncheckedValue'] : "0";
                $default = isset($s['default']) ? $s['default'] : "0";

                PrintSettingCheckbox($s['description'], $setting, $restart, $reboot, $checkedValue, $uncheckedValue, $plugin, $callback, $default, '', $s);
                break;
            case 'time':
                $size = 8;
                $maxlength = 8;
                $default = isset($s['default']) ? $s['default'] : '';
                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', 'text', $s);
                break;
            case 'date':
                $size = 10;
                $maxlength = 10;
                $default = isset($s['default']) ? $s['default'] : '';
                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', 'text', $s);
                break;
            case 'text':
                $size = isset($s['size']) ? $s['size'] : 32;
                $maxlength = isset($s['maxlength']) ? $s['maxlength'] : 32;
                $default = isset($s['default']) ? $s['default'] : "";

                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', 'text', $s);
                break;
            case 'color':
                $size = 7;
                $maxlength = 7;
                $default = isset($s['default']) ? $s['default'] : "800000";

                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', 'color', $s);
                break;
            case 'password':
                $size = isset($s['size']) ? $s['size'] : 32;
                $maxlength = isset($s['maxlength']) ? $s['maxlength'] : 32;
                $default = isset($s['default']) ? $s['default'] : "";

                PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $plugin, $default, $callback, '', "password", $s);

                echo "<i class='fas fa-eye' id='$setting" . "HideShow' onClick='TogglePasswordHideShow(\"" . $setting . "\");'></i>";
                break;
            case 'number':
                $min = isset($s['min']) ? $s['min'] : 0;
                $max = isset($s['max']) ? $s['max'] : 99;
                $step = isset($s['step']) ? $s['step'] : 1;
                $default = isset($s['default']) ? $s['default'] : "0";

                PrintSettingTextSaved($setting, $restart, $reboot, $max, $min, $plugin, $default, $callback, '', 'number', $s);
                break;
            default:
                printf("FIXME, handle %s setting type for %s\n", $s['type'], $setting);
                break;
        }

        if ($suffix != '') {
            echo $suffix . ' ';
        }

        if ($textOnRight) {
            echo "</div><div class='col-md-auto'><span>" . $s['description'] . "</span> ";
            PrintIcon($level);
        }

        PrintToolTip($setting);

        if ($textOnRight) {
            echo "</div></div>\n";
        } else {
            echo "</div></div>\n";
        }

    }
}

function PrintPluginSettingGroup($plugin, $group, $appendData = "", $prependData = "", $indent = 1)
{
    PrintSettingGroup($group, $appendData, $prependData, $indent, $plugin);
}

function PrintSettingGroup($group, $appendData = "", $prependData = "", $indent = 1, $plugin = "", $callback = "", $heading = true)
{
    global $settings;
    global $settingGroups;

    LoadSettingInfos();

    if ($plugin != '') {
        LoadPluginSettingInfos($plugin);
    }

    if (!isset($settingGroups[$group])) {
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
        if ($heading == true) {
            echo "<h2>" . $g['description'] . "</h2>\n";
        }
        echo "<div class='container-fluid settingsTable ";

        if ($indent) {
            echo "settingsGroupTable'>\n";
        } else {
            echo "'>\n";
        }

        if ($prependData != "") {
            if (preg_match("/<div class=row>/", $prependData)) {
                echo $prependData;
            } else {
                echo "<div class=row><div class='col-md' colspan=2>$prependData</div></div>\n";
            }

        }

        foreach ($g['settings'] as $setting) {
            if ($plugin != "") {
                PrintPluginSetting($plugin, $setting, $callback);
            } else {
                PrintSetting($setting, $callback);
            }

        }

        if ($appendData != "") {
            if (preg_match("/<div class=row>/", $appendData)) {
                echo $appendData;
            } else {
                echo "<div class=row><div class='col-md' colspan=2>$appendData</div></div>\n";
            }

        }

        echo "</div><br>\n";
    }
}

function PrintPluginSettingGroupTable($plugin, $group, $appendData = "", $prependData = "", $indent = 1)
{
    PrintSettingGroupTable($group, $appendData, $prependData, $indent, $plugin);
}

function PrintSettingGroupTable($group, $appendData = "", $prependData = "", $indent = 1, $plugin = "")
{
    echo "<div class='settingsTable " . $group . "SettingsTable'>\n";
    PrintSettingGroup($group, $appendData, $prependData, $indent, $plugin);
    echo "</div>\n";
}

function PrintSettingCheckbox($title, $setting, $restart = 1, $reboot = 0, $checkedValue, $uncheckedValue, $pluginName = "", $callbackName = "", $defaultValue = 0, $desc = "", $sData = array())
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

    if ($callbackName != "" && ($callbackName[strlen($callbackName) - 1] != ")")) {
        $callbackName = $callbackName . "('$setting');";
    }

    $changedFunction = preg_replace('/\./', '', $setting . "Changed");

    echo "<script>\n";
    if (isset($sData['children'])) {
        echo "function Update$setting" . "Children(mode) {
	var checked = 0;
	if ($('#$escSetting').is(':checked')) {
		checked = 1;
    }

    if (checked)
        $('.$escSetting' + 'Child').show();
    else
        $('.$escSetting' + 'Child').hide();

";
        foreach ($sData['children'] as $k => $v) {
            if ($k == "1") {
                echo "if (checked) {\n";
                echo "    if (mode != 2) {\n";
                foreach ($v as $name) {
                    echo "    if ((mode == 0) || (hiddenChildren.$name != 1))\n";
                    echo "        $('#" . $name . "Row').show();\n";
                }
                echo "    }\n";
                echo "} else {\n";
                echo "    if (mode != 1) {\n";
                foreach ($v as $name) {
                    echo "$('#" . $name . "Row').hide();\n";
                    echo "hiddenChildren.$name = 1;\n";
                }
                echo "    }\n";
                echo "}\n";
            }
        }

        echo "}\n\n";
    }

    echo "
function " . $changedFunction . "() {
	var value = '$uncheckedValue';
	var checked = 0;
	$('#$escSetting').parent().parent().addClass('loading');
	if ($('#$escSetting').is(':checked')) {
		checked = 1;
		value = '$checkedValue';
	}

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			if (checked)
				$.jGrowl('$title Enabled',{themeState:'success'});
			else
				$.jGrowl('$title Disabled',{themeState:'detract'});
			$settingsName" . "['$setting'] = value;
";

    if ($restart) {
        echo "SetRestartFlag($restart);\n";
    }

    if ($reboot) {
        echo "SetRebootFlag();\n";
    }

    if (isset($sData['children'])) {
        echo "Update$setting" . "Children(0);\n";
    }

    echo "
			$callbackName
			$('#$escSetting').parent().parent().removeClass('loading');
            if (checked)
                $('.$escSetting' + 'Child').show();
            else
                $('.$escSetting' + 'Child').hide();

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

    if (isset($sData['children'])) {
        echo "class='parentSetting' ";
    }

    IfSettingEqualPrint($setting, $checkedValue, "checked", $pluginName, $defaultValue);

    if (isset($sData['onChange'])) {
        echo " onChange='" . $sData['onChange'] . "();'";
    } else {
        echo " onChange='" . $changedFunction . "();'";
    }

    if ($desc != "") {
        echo '>' . $desc . "</input>\n";
    } else {
        echo " />\n";
    }

}

function PrintSettingSelectInternal($title, $setting, $restart = 1, $reboot = 0, $defaultValue, $values, $pluginName = "", $callbackName = "", $changedFunction = "", $sData = array(), $multiple = false)
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

    if ($callbackName != "" && ($callbackName[strlen($callbackName) - 1] != ")")) {
        $callbackName = $callbackName . "();";
    }

    if ($changedFunction == "") {
        $changedFunction = preg_replace('/\./', '', $setting . "Changed");
    }

    echo "<script>\n";

    if (isset($sData['children'])) {
        echo "function Update$setting" . "Children(mode) {
    var val = $('#$escSetting').val();
";
        foreach ($sData['children'] as $k => $v) {
            echo "if (val == '$k') {\n";
            echo "    if (mode != 2) {\n";
            foreach ($v as $name) {
                echo "    if ((mode == 0) || (hiddenChildren.$name != 1))\n";
                echo "        $('#" . $name . "Row').show();\n";
            }
            echo "    }\n";
            echo "} else {\n";
            echo "    if (mode != 1) {\n";
            foreach ($v as $name) {
                echo "$('#" . $name . "Row').hide();\n";
                echo "hiddenChildren.$name = 1;\n";
            }
            echo "    }\n";
            echo "}\n";
        }

        echo "}\n\n";
    }

    if (isset($sData['reloadOther'])) {
        echo "function ReloadOther$setting" . "() {
";
        foreach ($sData['reloadOther'] as $sname) {
            echo "    ReloadSettingOptions('$sname');\n";
        }

        echo "}\n\n";
    }

    echo "
function " . $changedFunction . "() {
	var value = encodeURIComponent($('#$escSetting').val());

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			$.jGrowl('$title saved',{themeState:'success'});
			$settingsName" . "['$setting'] = value;
			$callbackName
";

    if ($restart) {
        echo "SetRestartFlag($restart);\n";
    }

    if ($reboot) {
        echo "SetRebootFlag();\n";
    }

    if (isset($sData['children'])) {
        echo "Update$setting" . "Children(0);\n";
    }

    if (isset($sData['reloadOther'])) {
        echo "ReloadOther$setting" . "();\n";
    }

    echo "
		}).fail(function() {
			DialogError('$title', 'Failed to save $title');
		});
}
</script>
";

    if (isset($sData['onChange'])) {
        echo "<select id='$setting' onChange='" . $sData['onChange'] . "();' ";
    } else {
        echo "<select id='$setting' onChange='" . $changedFunction . "();' ";
    }

    if ($multiple) {
        if (isset($sData['children'])) {
            echo "multiple class='parentSetting multiSelect' ";
        } else {
            echo "multiple class='multiSelect' ";
        }
    } else if (isset($sData['children'])) {
        echo "class='parentSetting' ";
    }

    echo ">\n";

    foreach ($values as $key => $value) {
        echo "<option value='$value'";

        if (isset($pluginSettings[$setting]) || isset($settings[$setting])) {
            if ($multiple) {
                IfSettingInListPrint($setting, $value, " selected", $pluginName);
            } else {
                IfSettingEqualPrint($setting, $value, " selected", $pluginName);
            }
        } else if ($value == $defaultValue) {
            echo " selected";
        }

        echo ">$key</option>\n";
    }

    echo "</select>\n";
}
function PrintSettingMultiSelect($title, $setting, $restart = 1, $reboot = 0, $defaultValue, $values, $pluginName = "", $callbackName = "", $changedFunction = "", $sData = array())
{
    PrintSettingSelectInternal($title, $setting, $restart, $reboot, $defaultValue, $values, $pluginName, $callbackName, $changedFunction, $sData, true);
}
function PrintSettingSelect($title, $setting, $restart = 1, $reboot = 0, $defaultValue, $values, $pluginName = "", $callbackName = "", $changedFunction = "", $sData = array())
{
    PrintSettingSelectInternal($title, $setting, $restart, $reboot, $defaultValue, $values, $pluginName, $callbackName, $changedFunction, $sData, false);
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

    if (isset($settings[$setting])) {
        echo $settings[$setting];
    } elseif (isset($pluginSettings[$setting])) {
        echo $pluginSettings[$setting];
    } else {
        echo $defaultValue;
    }

    echo "\">\n";
}

function PrintSettingTextSaved($setting, $restart = 1, $reboot = 0, $maxlength = 32, $size = 32, $pluginName = "", $defaultValue = "", $callbackName = "", $changedFunction = "", $inputType = "text", $sData = array())
{
    global $settings;
    global $pluginSettings;

    $subType = $inputType;
    if ($inputType == 'color') {
        $inputType = 'text';
    }
    if ($inputType == 'datalist') {
        $inputType = 'text';
    }

    $plugin = "";
    $settingsName = "settings";

    $escSetting = preg_replace('/\./', '\\\\\\\\.', $setting);

    if ($pluginName != "") {
        $plugin = "Plugin";
        $settingsName = "pluginSettings";
    }

    if ($callbackName != "" && ($callbackName[strlen($callbackName) - 1] != ")")) {
        $callbackName = $callbackName . "();";
    }

    if ($changedFunction == "") {
        $changedFunction = preg_replace('/\./', '', $setting . "Changed");
    }

    $maxTag = 'maxlength';
    $sizeTag = 'size';
    if ($inputType == 'number') {
        $maxTag = 'max';
        $sizeTag = 'min';
    }

    echo "<script>\n";
    if (isset($sData['children'])) {
        echo "function Update$setting" . "Children(mode) {
    var val = $('#$escSetting').val();
";
        foreach ($sData['children'] as $k => $v) {
            echo "if (val != '') {\n";
            echo "    if (mode != 2) {\n";
            foreach ($v as $name) {
                echo "    if ((mode == 0) || (hiddenChildren.$name != 1))\n";
                echo "        $('#" . $name . "Row').show();\n";
            }
            echo "    }\n";
            echo "} else {\n";
            echo "    if (mode != 1) {\n";
            foreach ($v as $name) {
                echo "$('#" . $name . "Row').hide();\n";
                echo "hiddenChildren.$name = 1;\n";
            }
            echo "    }\n";
            echo "}\n";
        }

        echo "}\n\n";
    }

    echo "
    function " . $changedFunction . "() {
        var value = $('#$escSetting').val();
";

    if (isset($sData['regex']) && isset($sData['regexDesc'])) {
        echo "
        if (!RegexCheckData(\"" . $sData['regex'] . "\", value, \"" . $sData['regexDesc'] . "\", '$inputType' == 'password')) {
            $('#" . $escSetting . "').focus();
            return;
        }
";
    }

    echo "
        $.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + encodeURIComponent(value))
        .done(function() {
              $.jGrowl('$setting Saved',{themeState:'success'});
              $settingsName" . "['$setting'] = value;
              ";

    if ($restart) {
        echo "SetRestartFlag($restart);\n";
    }

    if ($reboot) {
        echo "SetRebootFlag();\n";
    }

    if (isset($sData['children'])) {
        echo "Update$setting" . "Children(0);\n";
    }

    echo "
              $callbackName
              CheckRestartRebootFlags();
              }).fail(function() {
                      DialogError('$setting', 'Failed to save $setting');
              });
    }
    </script>
";

    echo "<input type='$inputType' id='$setting' $maxTag='$maxlength' $sizeTag='$size' ";

    if (isset($sData['step']) && $sData['step'] != 1) {
        echo "step='" . $sData['step'] . "' ";
    }

    if (isset($sData['onChange'])) {
        echo "onChange='" . $sData['onChange'] . "();' value=\"";
    } else {
        echo "onChange='" . $changedFunction . "();' value=\"";
    }

    $curValue = "";
    if ((isset($sData['alwaysReset'])) && ($sData['alwaysReset'] == 1)) {
        $curValue = $defaultValue;
    } else {
        if (isset($settings[$setting])) {
            $curValue = $settings[$setting];
        } elseif (isset($pluginSettings[$setting])) {
            $curValue = $pluginSettings[$setting];
        } else {
            $curValue = $defaultValue;
        }

    }
    echo $curValue;

    echo "\" \n";

    if (isset($sData['children'])) {
        echo "class='parentSetting' ";
    }
    if (isset($sData['type'])) {
        if ($sData['type'] == 'time') {
            echo "class='time' autocomplete='off' ";
        }
        if ($sData['type'] == 'date') {
            echo "class='date' autocomplete='off' ";
        }
        if ($sData['type'] == 'datalist') {
            echo "list='" . $setting . "_list'";
        }
    }
    echo ">\n";
    if ($subType == 'datalist') {
        $options = $sData["options"];
        echo "<datalist id='" . $setting . "_list'>\n";
        foreach ($options as $option => $value) {
            echo "<option value='" . $option . "'>" . $value . "</option>\n";
        }
        echo "</datalist>\n";
    }
    if ($subType == 'color') {
        echo "<span id='$setting" . "_colorBox' class='color-box' style='background-color: #" . $curValue . ";'></span>";
        echo "<script>";
        echo "var $setting" . "SaveTimer = null;\n";
        echo "$('#$setting" . "_colorBox').colpick({ layout: 'rgbhex', color: '" . $curValue . "', submit: false, onChange: function(hsb,hex,rgb,el,bySetColor) { clearTimeout($setting" . "SaveTimer); $(el).css('background-color', '#'+hex); $('#$setting').val(hex); $setting" . "SaveTimer = setTimeout( function() { " . $changedFunction . "(); }, 500)}}).css('background-color', '#" . $curValue . "');";
        echo "</script>";
    }
}

function PrintSettingPasswordSaved($setting, $restart = 1, $reboot = 0, $maxlength = 32, $size = 32, $pluginName = "", $defaultValue = "", $callbackName = "", $changedFunction = "")
{
    $sData = array();
    PrintSettingTextSaved($setting, $restart, $reboot, $maxlength, $size, $pluginName, $defaultValue, $callbackName, $changedFunction, "password", $sData);
    echo "&nbsp;<input id='" . $setting . "_showHideButton' type='button' class='buttons' value='Show' onclick='" . $setting . "_showHidePassword()'>";

    ?>
<script>
function <?=$setting?>_showHidePassword() {
  var x = document.getElementById("<?=$setting?>");
  var b = document.getElementById("<?=$setting?>_showHideButton");
  if (x.type === "password") {
    x.type = "text";
    b.value = "Hide";
  } else {
    x.type = "password";
    b.value = "Show";
  }
}
</script>
<?
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

    if ($callbackName != "" && ($callbackName[strlen($callbackName) - 1] != ")")) {
        $callbackName = $callbackName . "();";
    }

    $saveFunction = preg_replace('/\./', '', $setting . "Changed");

    echo "
<script>
function " . $saveFunction . "() {
	var value = $('#$escSetting').val();

	$.get('fppjson.php?command=set" . $plugin . "Setting&plugin=$pluginName&key=$setting&value=' + value)
		.done(function() {
			$.jGrowl('$title saved',{themeState:'success'});
			$settingsName" . "['$setting'] = value;
			$callbackName
";

    if ($restart) {
        echo "SetRestartFlag($restart);\n";
    }

    if ($reboot) {
        echo "SetRebootFlag();\n";
    }

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
function get_sequence_file_info($mediaName)
{
    global $settings;

    $total_duration = 0;
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
        'seqMediaName' => null,
    );
    //Make sure it exists first
    if (!empty($mediaName) & file_exists($filename)) {
        //Get the filesize
        $media_filesize = filesize($filename);
        //Read the sequence
        $file_handle = fopen($filename, "r");
        //Read the first 28 bytes for the header
        while (($data = fread($file_handle, 28)) !== false && strlen($data) == 28) {

            //Break data down and split out the bits, ignore my horrible decoding method lol
            $all_data['seqFormatID'] = unpack("A*", substr($data, 0, 4))[1]; //uint32 - magic cookie -- Results in FSEQ
            $all_data['seqChanDataOffset'] = unpack("S*", substr($data, 4, 2))[1]; //uint32 - DataOffset -- only 2 bytes so really uint16 -- checks out on test sequence (1040) & FPPD log output
            $all_data['seqMinorVersion'] = unpack("C*", substr($data, 6, 1))[1]; //uint8 - Minor Version -- May not be correct decoding - results in 0
            $all_data['seqMajorVersion'] = unpack("C*", substr($data, 7, 1))[1]; //uint8 - Major version -- -- May not be correct decoding - results in 1... Version 1.0? seems correct
            //build version
            $all_data['seqVersion'] = $all_data['seqMajorVersion'] . "." . $all_data['seqMinorVersion'];

            $all_data['seqFixedHeaderSize'] = unpack("S*", substr($data, 8, 2))[1]; //uint16 - FixedHeader: 28 - checks out with FPPD log output
            $all_data['seqStepSize'] = unpack("L*", substr($data, 10, 4))[1]; //uint32 - StepSize
            $all_data['seqNumPeriods'] = unpack("L*", substr($data, 14, 4))[1]; //uint32 - NumSteps
            $all_data['seqStepTime'] = unpack("S*", substr($data, 18, 2))[1]; //uint16 - StepTime (eg 50ms)
            $all_data['seqNumUniverses'] = unpack("S*", substr($data, 20, 2))[1]; //uint16 - Number of Universes -- Def: 0 //Ignored by Pi Player
            $all_data['seqUniverseSize'] = unpack("S*", substr($data, 22, 2))[1]; //uint16 - Size of Universes -- Def: 0 //Ignored by Pi Player
            $all_data['seqGamma'] = unpack("C*", substr($data, 24, 1))[1]; //uint8 - Gamma -- Def: 1?
            $all_data['seqColorEncoding'] = unpack("C*", substr($data, 25, 1))[1]; //uint8 - Type  -- Def: 2? -- RGB Channels //0=encoded, 1=linear, 2=RGB
            //            $all_data['seqUnusedField'] = unpack("S*", substr($data, 26, 2))[1]; //uint16 - unused

            //VARIABLE HEADER
            //to read the variable header, seek to offset 28, then read data between 28 and seqChanDataOffset..
            fseek($file_handle, 28);
            //only read first 128 bytes just to catch everything, shouldn't need more than this for our purpose
            $seek_data = fread($file_handle, 128); //$all_data['seqChanDataOffset'] - 28
            //make sure we have data
            if ($seek_data !== false && strlen($seek_data) == 128) {
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

//                    $all_data['variable_header_len'] = $variable_header_len;
                    //                    $all_data['variable_header_type_of_code'] = $variable_header_type_of_code;
                    //                    $all_data['variable_header_media_filename'] = unpack("A*", substr($seek_data, $media_filename_data_offset, $remaining_header_len));
                    //                    $all_data['variable_header_media_offset'] = $media_filename_data_offset;
                    //                    $all_data['variable_header_filename_len'] = $gs_ident;
                    //                    $all_data['variable_header_filename_actual_len'] = strlen($sequenceMediaName);

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
        if ($file_handle === false) {
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
    $file_path = $config_dir . "/" . $cache_file;
    $time = 86400 * 30; //seconds - cache for 24hrs * 30days
    $duration_cache = array();
    $return_duration = null;

    if (!file_exists($file_path) || (time() - filemtime($file_path) > $time)) {
        // cache doesn't exist or expired so lets create it and insert our media entry
        if ($duration_seconds !== null) {
            //put the media duration into the cache, but only if it isn't null
            $duration_cache[$media] = array('filesize' => $filesize, 'duration' => $duration_seconds);

            $return_duration = $duration_seconds;
            file_put_contents($file_path, json_encode($duration_cache, JSON_PRETTY_PRINT), LOCK_EX);
        }
    } else {
        //else cache exists and is valid, replaces/append duration to it
        $duration_cache = file_get_contents($file_path);
        if ($duration_cache !== false && !empty($duration_cache)) {
            $duration_cache = json_decode($duration_cache, true);
            //if file hashes are the same - then it's the same file
            if (array_key_exists($media, $duration_cache) && $duration_cache[$media]['filesize'] == $filesize) {
                //Key exists, then return the cached duration
                $return_duration = $duration_cache[$media]['duration'];
            } else if ($duration_seconds !== null) {
                //put the media duration into the cache, but only if it isn't null
                $duration_cache[$media] = array('filesize' => $filesize, 'duration' => $duration_seconds);
                $return_duration = $duration_seconds;

                file_put_contents($file_path, json_encode($duration_cache, JSON_PRETTY_PRINT), LOCK_EX);
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
function human_filesize($path)
{
    // cannot use filesize($path) as that returns a signed 32bit number so maxes out at 2GB
    $kbytes = trim(shell_exec("du -k \"" . $path . "\" | cut -f1 "));
    if (strlen($kbytes) < 3) {
        $bytes = filesize($path);
        $sz = 'BKMGTP';
        $factor = floor((strlen($bytes) - 1) / 3);
        if ($factor) {
            return sprintf("%.2f", $bytes / pow(1024, $factor)) . @$sz[$factor] . ($factor > 0 ? "B" : "");
        }

        return sprintf("%d", $bytes / pow(1024, $factor)) . @$sz[$factor] . ($factor > 0 ? "B" : "");
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
function human_playtime($total_duration)
{
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
function get_server_memory_usage()
{
    global $settings;
    if (file_exists("/proc/meminfo")) {
        $fh = fopen('/proc/meminfo', 'r');
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
    } else if ($settings["Platform"] == "MacOS") {
        $output = exec("memory_pressure | grep System-wide");
        $array = explode(":", $output);
        $memory_usage = trim(rtrim($array[1], "%"));
        $memory_usage = 100 - $memory_usage;
    } else {
        $memory_usage = 0;
    }
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
            //            $cache_data[$cache_name] = array('data' => $data_to_cache);
            $cache_data_return = $data_to_cache;

//            exec("echo \"$data_to_cache\" | sudo tee $file_path", $output, $return_val);
            file_put_contents($file_path, $data_to_cache, LOCK_EX);
        }
    } else {
        //else cache exists and is valid, replaces/append duration to it
        $cache_data_contents = file_get_contents($file_path);

//        $handle = fopen($file_path, 'r');
        //        $cache_data_contents = trim(fread($handle,filesize($file_path)));
        //        fclose($handle);

        if ($cache_data_contents !== false && !empty($cache_data_contents)) {
//            $cache_data = json_decode($cache_data, true);
            //if file hashes are the same - then it's the same file
            //if (array_key_exists($cache_name, $cache_data)) {
            //Key exists, then return the cached duration
            //            $cache_data_return = $cache_data[$cache_name]['data'];
            $cache_data_return = $cache_data_contents;
        } else if ($data_to_cache !== null || $data_to_cache !== "") {
            //put the supplied data into the cache
            //            $cache_data[$cache_name] = array('data' => $data_to_cache);
            $cache_data_return = $data_to_cache;

//            exec("echo \"$data_to_cache\" | sudo tee $file_path", $output, $return_val);
            file_put_contents($file_path, $data_to_cache, LOCK_EX);
        }
    }
    return $cache_data_return;
}

function get_kernel_version()
{
    $kernel_version = "";
    $cachefile_name = "kernel_version";
    $cache_age = 86400;

    $cached_data = file_cache($cachefile_name, null, $cache_age);
    if ($cached_data == null) {
        $kernel_version = exec("uname -r");
        //cache result
        file_cache($cachefile_name, $kernel_version, $cache_age);
    } else {
        $kernel_version = $cached_data;
    }

    return $kernel_version;
}

function get_cpu_stats()
{
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
    return array(0, 0, 0, 0, 0, 0, 0);
}

/**
 * Returns average CPU usage
 * @return mixed
 */
function get_server_cpu_usage()
{
    global $settings;
    if ($settings["Platform"] == "MacOS") {
        $output = exec("ps -A -o %cpu | awk '{s+=$1} END {print s}'");
        return floatval($output);
    }
    if (!file_exists("/tmp/cpustats.txt")) {
        $ostats = get_cpu_stats();
        $vs = sprintf("%d %d %d %d %d %d %d", $ostats[0], $ostats[1], $ostats[2], $ostats[3], $ostats[4], $ostats[5], $ostats[6]);
        @file_put_contents("/tmp/cpustats.txt", $vs, LOCK_EX);
        usleep(10000);
    } else {
        $statLine = @file_get_contents("/tmp/cpustats.txt");
        $ostats = explode(" ", trim($statLine));
    }
    $stats = get_cpu_stats();
    $vs = sprintf("%d %d %d %d %d %d %d", $stats[0], $stats[1], $stats[2], $stats[3], $stats[4], $stats[5], $stats[6]);
    @file_put_contents("/tmp/cpustats.txt", $vs, LOCK_EX);

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
    if ($total > 0) {
        $val = 100.0 - ($val / $total);
    }

    return $val;
}

/**
 * Returns server uptime
 * @param bool $uptime_value_only
 * @return null|string|string[]
 */
function get_server_uptime($uptime_value_only = false)
{
    $uptime = exec("uptime", $output, $return_val);
    if ($return_val != 0) {
        $uptime = "";
    }

    unset($output);
    $uptime = preg_replace('/[0-9]+ users?, /', '', $uptime);
    if ($uptime_value_only) {
        $uptime_portion = explode(",", $uptime, 2)[0];
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
function get_fpp_head_version()
{
    $fpp_head_version = "Unknown";
    $cachefile_name = "git_fpp_head_version";
    $cache_age = 90;

    $cached_data = file_cache($cachefile_name, null, $cache_age);
    if ($cached_data == null) {
        $fpp_head_version = exec("git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ describe", $output, $return_val);
        if ($return_val != 0) {
            $fpp_head_version = "Unknown";
        }

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
function get_git_branch()
{
    $git_branch = "";
    $cachefile_name = "git_branch";
    $cache_age = 90;

    $cached_data = file_cache($cachefile_name, null, $cache_age);
    if ($cached_data == null) {
        $git_branch = exec("git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
        if ($return_val != 0) {
            $git_branch = "Unknown";
        }

        unset($output);
        //cache result
        file_cache($cachefile_name, $git_branch, $cache_age);
    } else {
        $git_branch = $cached_data;
    }

    return $git_branch;
}

/**
 * Returns the version of the local Git branch
 * @return string
 */
function get_local_git_version()
{
    $git_version = "Unknown";
    $cachefile_name = "local_git_version";
    $cache_age = 20;

    $cached_data = file_cache($cachefile_name, null, $cache_age);
    if ($cached_data == null) {
        $git_version = exec("git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ rev-parse --short=7 HEAD", $output, $return_val);
        if ($return_val != 0) {
            $git_version = "Unknown";
        }

        unset($output);
        //cache result
        file_cache($cachefile_name, $git_version, $cache_age);
    } else {
        $git_version = $cached_data;
    }

    return $git_version;
}

/**
 * Returns version of the remote Git branch for the supplied branch
 * @return string
 */
function get_remote_git_version($git_branch)
{
    global $settings;

    $git_remote_version = "Unknown";

    $origin = "github.com";
    if (isset($settings['UpgradeSource'])) {
        $origin = $settings['UpgradeSource'];
    }

    if (!empty($git_branch) || strtolower($git_branch) != "unknown") {
        $cachefile_name = "git_" . $git_branch;
        $cache_age = 90;
        $git_remote_version = "Unknown";

        //Check the cache for git_<branch>, if null is returned no cache file exists or it's expired, so then off to github
        $cached_data = file_cache($cachefile_name, null, $cache_age);
        if ($cached_data == null) {
            //if for some reason name resolution fails ping will take roughly 10 seconds to return (Default DNS Timeout 5 seconds x 2 retries)
            //to try work around this ping the google public DNS @ 8.8.8.8 (to skip DNS) waiting for a reply for max 1 second, if that's ok we have a route to the internet, then it's highly likely DNS will also work
            $return_val = 0;
            if ($origin == 'github.com') {
                $google_dns_ping = exec("ping -q -c 1 -W 1 8.8.8.8 > /dev/null", $output, $return_val);
                unset($output);
            }
            if ($return_val == 0) {
                //Google DNS Ping success
                // this can take a couple seconds to complete so we'll cache it
                $git_remote_version = exec("(git --git-dir=" . $settings["fppDir"] . "/.git/ ls-remote -q -h origin $git_branch | awk '$1 > 0 { print substr($1,1,7)}')", $output, $return_val);
                if ($return_val != 0) {
                    $git_remote_version = "Unknown";
                }

                unset($output);

                // If we get back a blank version, this clone may be setup to
                // use ssh to access github, so fallback and check for a
                // 'github' origin which can be setup to use https://
                if ($git_remote_version == "") {
                    $git_remote_version = exec("ping -q -c 1 github.com > /dev/null && (git --git-dir=" . $settings["fppDir"] . "/.git/ ls-remote -q -h github $git_branch | awk '$1 > 0 { print substr($1,1,7)}')", $output, $return_val);
                    if ($return_val != 0) {
                        $git_remote_version = "Unknown";
                    }

                    unset($output);
                }
            } else {
                //Google DNS Ping fail - return unknown
                $git_remote_version = "Unknown";
            }

            //cache result
            file_cache($cachefile_name, $git_remote_version, $cache_age);
        } else {
            //return the cached version
            $git_remote_version = $cached_data;
        }
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
 * Generates appropriate files and settings for exim4
 */
function ApplyEmailConfig()
{
    global $exim4Directory;
    global $settings;

    if (isset($settings['HostName'])) {
        $hostname = $settings['HostName'];
    } else {
        $hostname = 'FPP';
    }

    if (isset($settings['emailserver'])) {
        $emailserver = $settings['emailserver'];
    } else {
        $emailserver = 'smtp.gmail.com';
    }

    if (isset($settings['emailport'])) {
        $emailport = $settings['emailport'];
    } else {
        $emailport = 587;
    }

    if (isset($settings['emailuser'])) {
        $emailuser = $settings['emailuser'];
    } else {
        $emailuser = '';
    }

    if (isset($settings['emailpass'])) {
        $emailpass = $settings['emailpass'];
    } else {
        $emailpass = '';
    }

    if (isset($settings['emailfromuser'])) {
        $emailfromuser = $settings['emailfromuser'];
    } else {
        $emailfromuser = $emailuser;
    }

    if (isset($settings['emailfromtext'])) {
        $emailfromtext = $settings['emailfromtext'];
    } else {
        $emailfromtext = 'FPP - ' . $hostname;
    }

    if (isset($settings['emailtoemail'])) {
        $emailtoemail = $settings['emailtoemail'];
    } else {
        $emailtoemail = '';
    }

    if ($emailtoemail == '') {
        return;
    }

    $exim4Conf = sprintf(
        "dc_eximconfig_configtype='smarthost'\n" .
        "dc_other_hostnames='%s'\n" .
        "dc_local_interfaces='127.0.0.1'\n" .
        "dc_readhost=''\n" .
        "dc_relay_domains=''\n" .
        "dc_minimaldns='false'\n" .
        "dc_relay_nets=''\n" .
        "dc_smarthost='%s::%d'\n" .
        "CFILEMODE='644'\n" .
        "dc_use_split_config='true'\n" .
        "dc_hide_mailname='false'\n" .
        "dc_mailname_in_oh='true'\n" .
        "dc_localdelivery='mail_spool'\n",
        $hostname, $emailserver, $emailport);

    $fp = fopen($exim4Directory . '/update-exim4.conf.conf', 'w');
    fwrite($fp, $exim4Conf);
    fclose($fp);

    $fp = fopen($exim4Directory . '/passwd.client', 'w');
    fwrite($fp, "# password file used when the local exim is authenticating to a remote host as a client.\n");
    fwrite($fp, "#\n");
    if (($emailuser != '') && ($emailpass != '')) {
        fwrite($fp, $emailserver . ":" . $emailuser . ":" . $emailpass . "\n");

        $v = dns_get_record($emailserver, DNS_CNAME);
        foreach ($v as $cname) {
            if (isset($cname["target"])) {
                fwrite($fp, $cname["target"] . ":" . $emailuser . ":" . $emailpass . "\n");
            }
        }
    }
    fclose($fp);

    $fp = fopen($exim4Directory . '/aliases', 'w');
    fwrite($fp, "mailer-daemon: postmaster\npostmaster: root\nnobody: root\nhostmaster: root\nusenet: root\nnews: root\nwebmaster: root\nwww: root\nftp: root\nabuse: root\nnoc: root\nsecurity: root\nroot: fpp\n");
    fwrite($fp, "pi: " . $emailtoemail . "\n");
    fwrite($fp, "fpp: " . $emailtoemail . "\n");
    fclose($fp);

    $fp = fopen($exim4Directory . '/email-addresses', 'w');
    fwrite($fp, "fpp: " . $emailfromuser . "\n");
    fwrite($fp, "root: " . $emailfromuser . "\n");
    fclose($fp);

    # copy our conf files into place
    exec("sudo cp " . $exim4Directory . "/update-exim4.conf.conf /etc/exim4/");
    exec("sudo cp " . $exim4Directory . "/passwd.client /etc/exim4/");
    exec("sudo cp " . $exim4Directory . "/aliases /etc/");
    exec("sudo cp " . $exim4Directory . "/email-addresses /etc/");

    # update exim4 config using our generated conf files
    exec("sudo update-exim4.conf");

    # set the fpp user GECOS field
    $cmd = "sudo chfn -f \"" . $emailfromtext . "\" fpp";
    exec($cmd);

    if (file_exists("/.dockerenv")) {
        exec("sudo /etc/init.d/exim4 restart");
    } else {
        exec("sudo systemctl restart exim4.service");
    }
}

// This function is from:
// http://stackoverflow.com/questions/6054033/pretty-printing-json-with-php
function prettyPrintJSON($json)
{
    $result = '';
    $level = 0;
    $in_quotes = false;
    $in_escape = false;
    $ends_line_level = null;
    $json_length = strlen($json);

    for ($i = 0; $i < $json_length; $i++) {
        $char = $json[$i];
        $new_line_level = null;
        $post = "";
        if ($ends_line_level !== null) {
            $new_line_level = $ends_line_level;
            $ends_line_level = null;
        }
        if ($in_escape) {
            $in_escape = false;
        } else if ($char === '"') {
            $in_quotes = !$in_quotes;
        } else if (!$in_quotes) {
            switch ($char) {
                case '}':case ']':
                    $level--;
                    $ends_line_level = null;
                    $new_line_level = $level;
                    break;

                case '{':case '[':
                    $level++;
                case ',':
                    $ends_line_level = $level;
                    break;

                case ':':
                    $post = " ";
                    break;

                case " ":case "\t":case "\n":case "\r":
                    $char = "";
                    $ends_line_level = $new_line_level;
                    $new_line_level = null;
                    break;
            }
        } else if ($char === '\\') {
            $in_escape = true;
        }
        if ($new_line_level !== null) {
            $result .= "\n" . str_repeat("\t", $new_line_level);
        }
        $result .= $char . $post;
    }

    return $result;
}

function DisableOutputBuffering()
{
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
        if (ob_get_level() == $level) {
            break;
        }

    }

    // Disable apache output buffering/compression
    if (function_exists('apache_setenv')) {
        apache_setenv('no-gzip', '1');
        apache_setenv('dont-vary', '1');
    }

    ob_implicit_flush(true);
    flush();
}

function PrintToolTip($setting)
{
    global $settingInfos;
    if (empty($settingInfos)) {
        LoadSettingInfos();
    }

    if ((isset($settingInfos[$setting])) &&
        (isset($settingInfos[$setting]['tip']))) {
        $tip = $settingInfos[$setting]['tip'];
        echo "<img id='$setting" . "_img' title='$tip' src='images/redesign/help-icon.svg' class='icon-help'><span id='$setting" . "_tip' class='tooltip' style='display: none'>" . $tip . "</span>\n";
    }
}

function PrintAwesomeFree($code, $isLink = 0)
{
    echo "<span class='AwesomeFree";
    if ($isLink) {
        echo " AwesomeFreeLink";
    }

    echo "'>&#x$code;</span>";
}

#############
# Returns a PHP Array of Objects for each wifi interface describing strength
#############
function network_wifi_strength_obj()
{
    global $settings;
    $rc = array();
    if ($settings["Platform"] == "MacOS") {
        exec("/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -I", $status);
        $obj = new \stdClass();
        $obj->level = 100;
        foreach ($status as $line) {
            // do stuff with $line
            $line = trim($line);
            $array = explode(':', $line);
            $key = trim($array[0]);
            $value = trim($array[1]);
            if ($key == "agrCtlRSSI") {
                $obj->level = $value;
                $obj->unit = "dBm";
            }
            if ($key == "agrCtlNoise") {
                $obj->noise = $value;
            }
            if ($key == "lastTxRate") {
                $obj->rate = $value;
            }
        }
        if ($obj->level > -50) {
            $obj->desc = "excellent";
        } elseif ($obj->level > -60) {
            $obj->desc = "good";
        } elseif ($obj->level > -70) {
            $obj->desc = "fair";
        } else {
            $obj->desc = "weak";
        }
        array_push($rc, $obj);
    } else {
        $lines = file("/proc/net/wireless");
        #
        # Expected format
        #
        # face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22
        # wlan0: 0000   41.  -69.  -256        0      0      0   2042      0        0
        #
        # NOTE: If level is negative, then it is db.  If positive then it is quality %
        # https://www.intuitibits.com/2016/03/23/dbm-to-percent-conversion/ used for manual quality mapping
        #
        foreach ($lines as $cnt => $line) {
            if ($cnt > 1) {
                $parts = preg_split("/\s+/", trim($line));
                $obj = new \stdClass();
                $obj->interface = rtrim($parts[0], ":");
                $obj->link = intval($parts[2]);
                $obj->level = intval($parts[3]);
                $obj->unit = "dBm";
                $obj->noise = intval($parts[4]);
                $obj->desc = '';

                $output = exec("iwconfig " . $obj->interface . " | grep 'Link Quality' | cut -f2 -d: | cut -f2 -d= | awk '{print \$1}'");

                if ($output != '') {
                    $pparts = preg_split('/\//', trim($output));
                    $pct = intval($pparts[0]);
                    $max = 100;
                    if (count($pparts) > 1) {
                        $max = intval($pparts[1]);
                    }
                    if (($pct > 0) && ($max > 0)) {
                        if ($max != 100) {
                            $pct = intval($pct * 100.0 / $max);
                        }
                        $obj->pct = $pct;

                        if ($obj->level >= 0) {
                            $obj->unit = "pct";
                            $obj->level = $pct;
                        }

                        if ($obj->pct >= 79) {
                            $obj->desc = "excellent";
                        } elseif ($obj->pct > 66) {
                            $obj->desc = "good";
                        } elseif ($obj->pct > 48) {
                            $obj->desc = "fair";
                        } else {
                            $obj->desc = "weak";
                        }
                    }
                }

                if ($obj->desc == '') {
                    if ($obj->level >= 0) {
                        $obj->unit = "pct";
                        if ($obj->level >= 79) {
                            $obj->desc = "excellent";
                        } elseif ($obj->level > 66) {
                            $obj->desc = "good";
                        } elseif ($obj->level > 48) {
                            $obj->desc = "fair";
                        } else {
                            $obj->desc = "weak";
                        }
                    } else {
                        if ($obj->level > -50) {
                            $obj->desc = "excellent";
                        } elseif ($obj->level > -60) {
                            $obj->desc = "good";
                        } elseif ($obj->level > -70) {
                            $obj->desc = "fair";
                        } else {
                            $obj->desc = "weak";
                        }
                    }
                }

                array_push($rc, $obj);
            }
        }
    }
    return $rc;
}

#############
# Returns an array of just the network interface names
#############
function network_list_interfaces_array()
{
    global $settings;
    if ($settings["Platform"] == "MacOS") {
        $output = exec("ipconfig getiflist");
        $parts = preg_split("/\s+/", trim($output));
        return $parts;
    }
    $interfaces = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | grep -v tether ")));
    return $interfaces;
}

function network_list_interfaces_obj()
{
    global $settings;

    $output = array();
    global $settings;
    $rc = array();

    $wifiObj = network_wifi_strength_obj();
    if ($settings["Platform"] == "MacOS") {
        $output = exec("ipconfig getiflist");
        $parts = preg_split("/\s+/", trim($output));
        foreach ($parts as $cnt => $int) {
            exec("ipconfig getsummary " . $int, $config);
            $obj = new \stdClass();
            $obj->ifindex = $cnt;
            $obj->ifname = $int;
            $obj->addr_info = array();
            $obj->flags = array();
            array_push($obj->addr_info, new \stdClass());
            $lastKey = "";
            foreach ($config as $line) {
                // do stuff with $line
                $line = trim($line);
                $array = explode(':', $line);
                $key = trim($array[0]);
                if (count($array) > 1) {
                    $value = trim($array[1]);
                } else {
                    $value = "";
                }
                if ($key == "0" && $lastKey == "Addresses") {
                    $obj->addr_info[0]->family = "inet";
                    $obj->addr_info[0]->local = $value;
                    $obj->addr_info[0]->label = $int;
                    array_push($rc, $obj);
                } else if ($key == "InterfaceType" && $value == "WiFi") {
                    $wifiObj[0]->interface = $int;
                    $obj->wifi = $wifiObj[0];
                } else if ($key == "ConfigMethod" && $value == "DHCP") {
                    $obj->flags[] = "DYNAMIC";
                } else {
                    $lastKey = $key;
                }
            }
            unset($config);
        }
    } else {
        $cmd = "ip --json address show";
        exec($cmd, $output);
        $rc = json_decode(join(" ", $output), true);

        foreach ($rc as &$rec) {
            // Filter only ipv4 addresses
            $addrInfo = $rec['addr_info'];
            $newAddrInfo = array();
            foreach ($addrInfo as $ai) {
                if ($ai['family'] == 'inet') {
                    array_push($newAddrInfo, $ai);
                }

            }
            $rec['addr_info'] = $newAddrInfo;

            // Merge two objects
            foreach ($wifiObj as $wifi) {
                if ($rec["ifname"] == $wifi->interface) {
                    $rec["wifi"] = $wifi;
                }
            }
            $ifname = $rec["ifname"];
            $add = isset($rec["address"]) ? $rec["address"] : "";
            if ($ifname == "lo" || $ifname == "usb0" || $ifname == "usb1" || $ifname == "SoftAp0" || $add == "0.0.0.0" || $add == "::") {
                unset($rc[array_search($rec, $rc)]);
            }
            $cfgFile = $settings['configDirectory'] . "/interface." . $ifname;
            if (file_exists($cfgFile)) {
                $result = parse_ini_file($cfgFile);
                $rec["config"] = $result;
            }
        }
    }
    return array_values($rc);
}

// Return array of FPP Only systems
function getKnownFPPSystems()
{
    $backupHosts = array();
    $data = file_get_contents('http://localhost/api/fppd/multiSyncSystems');
    $arr = json_decode($data, true);

    if (array_key_exists("systems", $arr)) {
        foreach ($arr["systems"] as $i) {
            // FPP Systems are 0x01 to 0x80
            if ($i["typeId"] >= 1 && $i["typeId"] < 128) {
                $desc = $i["address"] . " - " . $i["hostname"];
                $backupHosts[$desc] = $i["address"];
            }
        }
        ksort($backupHosts);
    }
    return $backupHosts;
}

// Removes dangerious characters from file names
// Original idea from https://stackoverflow.com/questions/2021624/string-sanitizer-for-filename
function sanitizeFilename($file)
{
    $file = preg_replace("([^\w\s\d\-_~,;\[\]\(\).])", '', $file);
    // Replace ".." with "." to provent problems
    $file = preg_replace("([\.]{2,})", '.', $file);

    return $file;
}

function is_valid_domain_name($domain_name)
{
    return (preg_match("/^([a-z\d](-*[a-z\d])*)(\.([a-z\d](-*[a-z\d])*))*$/i", $domain_name) //valid chars check
         && preg_match("/^.{1,253}$/", $domain_name) //overall length check
         && preg_match("/^[^\.]{1,63}(\.[^\.]{1,63})*$/", $domain_name)); //length of each label
}

/////////////////////////////////////////////////////////////////////////////

// Returns the base directory of the git repo
// normally /opt/fpp
function gitBaseDirectory()
{
    return dirname(dirname(__FILE__));
}

function GetSystemInfoJsonInternal($simple = false)
{
    global $settings;

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $result = array();
    $result['HostName'] = $settings['HostName'];
    $result['HostDescription'] = !empty($settings['HostDescription']) ? $settings['HostDescription'] : "";
    $result['Platform'] = $settings['Platform'];
    $result['Variant'] = isset($settings['Variant']) ? $settings['Variant'] : '';
    $result['Mode'] = $settings['fppMode'];
    $result['Version'] = getFPPVersion();
    $result['Branch'] = getFPPBranch();
    $result['multisync'] = isset($settings['MultiSyncEnabled']) ? ($settings['MultiSyncEnabled'] == '1') : false;
    if ($result['Mode'] == "master") {
        $result['Mode'] = "player";
        $result['multisync'] = true;
    }
    if ($settings["Platform"] == "MacOS") {
        $result['OSRelease'] = trim(shell_exec("sw_vers -productVersion"));
        $result['OSVersion'] = $result['OSRelease'];
    } else {
        if (file_exists('/etc/fpp/rfs_version')) {
            $result['OSVersion'] = trim(file_get_contents('/etc/fpp/rfs_version'));
        }

        $os_release = "Unknown";
        if (file_exists("/etc/os-release")) {
            $info = parse_ini_file("/etc/os-release");
            if (isset($info["PRETTY_NAME"])) {
                $os_release = $info["PRETTY_NAME"];
            }

            unset($output);
        }
        $result['OSRelease'] = $os_release;
    }
    if (file_exists($settings['mediaDirectory'] . "/fpp-info.json")) {
        $content = file_get_contents($settings['mediaDirectory'] . "/fpp-info.json");
        $json = json_decode($content, true);
        $result['channelRanges'] = $json['channelRanges'];
        $result['majorVersion'] = $json['majorVersion'];
        $result['minorVersion'] = $json['minorVersion'];
        $result['typeId'] = $json['typeId'];
    }
    $output = array();
    exec($settings['fppDir'] . "/scripts/get_uuid", $output);
    $result['uuid'] = $output[0];

    if (!$simple) {
        //Get CPU & memory usage before any heavy processing to try get relatively accurate stat
        $result['Utilization']['CPU'] = get_server_cpu_usage();
        $result['Utilization']['Memory'] = get_server_memory_usage();
        $result['Utilization']['Uptime'] = get_server_uptime(true);

        $result['Kernel'] = get_kernel_version();
        $result['LocalGitVersion'] = get_local_git_version();
        $result['RemoteGitVersion'] = get_remote_git_version(getFPPBranch());

        $uploadDir = GetDirSetting("uploads"); // directory under media
        $result['Utilization']['Disk']["Media"]['Free'] = disk_free_space($uploadDir);
        $result['Utilization']['Disk']["Media"]['Total'] = disk_total_space($uploadDir);
        $result['Utilization']['Disk']["Root"]['Free'] = disk_free_space("/");
        $result['Utilization']['Disk']["Root"]['Total'] = disk_total_space("/");

        if (isset($settings['UpgradeSource'])) {
            $result['UpgradeSource'] = $settings['UpgradeSource'];
        } else {
            $result['UpgradeSource'] = 'github.com';
        }

        if ($settings["Platform"] != "MacOS") {
            $output = array();
            $IPs = array();
            exec("ip --json -4 address show", $output);
            //print(join("", $output));
            $ipAddresses = json_decode(join("", $output), true);
            foreach ($ipAddresses as $key => $value) {
                if ($value["ifname"] != "lo" && strpos($value["ifname"], 'usb') === false) {
                    foreach ($value["addr_info"] as $key2 => $value2) {
                        $IPs[] = $value2["local"];
                    }
                }
            }

            $result['IPs'] = $IPs;
        } else {
            $IPs = array();
            $output = exec("ipconfig getiflist");
            $parts = preg_split("/\s+/", trim($output));
            foreach ($parts as $cnt => $int) {
                exec("ipconfig getsummary " . $int, $config);
                $lastKey = "";
                foreach ($config as $line) {
                    // do stuff with $line
                    $line = trim($line);
                    $array = explode(':', $line);
                    $key = trim($array[0]);
                    if (count($array) > 1) {
                        $value = trim($array[1]);
                    } else {
                        $value = "";
                    }
                    if ($key == "0" && $lastKey == "Addresses") {
                        $IPs[] = $value;
                    } else {
                        $lastKey = $key;
                    }
                }
                unset($config);
            }
            $result['IPs'] = $IPs;
        }

    }
    return $result;
}

/**
 * Looks in a directory and reads file contents of files within it
 * @param $directory String directory to search in
 * @param $return_data Boolean switch to include file data
 * @param $sort_by_date bool Boolean sort results by date
 * @param $sort_order string Order to sort the dates by, DEFAULT is  oldest to newest
 * @return array Array of file names and respective data
 */
function read_directory_files($directory, $return_data = true, $sort_by_date = false, $sort_order = 'desc')
{
    $file_list = array();
    $file_data = false;

    if ($handle = opendir($directory)) {
        while (false !== ($file = readdir($handle))) {
            // do something with the file
            // note that '.' and '..' is returned even
            // if file isn't this directory or its parent, add it to the results
            // also must include ._* files as binary files that OSX may create
            if ($file[0] != "." || (strlen($file) > 1 && $file[1] != "." && $file[1] != "_")) {
                // collect the filenames & data
                if ($return_data == true) {
                    $file_data = explode("\n", file_get_contents($directory . '/' . $file));
                }

                $file_list[$file] = $file_data;
            }
        }
        closedir($handle);
    }

    //Sort the results if sort flag  is true
    if ($sort_by_date == true) {
        //Modified version of https://stackoverflow.com/questions/2667065/sort-files-by-date-in-php
        //newest to  oldest
        if ($sort_order === 'asc') {
            //uksort to sort by key & insert our directory variable
            uksort($file_list, function ($a, $b) use ($directory) {
                return filemtime($directory . "/" . $b) - filemtime($directory . "/" . $a);
            });
        }

        //oldest to newest
        if ($sort_order === 'desc') {

            //uksort to sort by key & insert our directory variable
            uksort($file_list, function ($a, $b) use ($directory) {
                return filemtime($directory . "/" . $a) - filemtime($directory . "/" . $b);
            });
        }
    }

    return $file_list;
}

/**
 * Makes a POST Call to the api/backups/configuration to generate a JSON Configuration backup with a option comment
 * @param $backup_comment string Optional Comment that will be inserted into the JSON backup file
 * @return bool
 */
function GenerateBackupViaAPI($backup_comment = "Created via API")
{
    $url = 'http://localhost/api/backups/configuration';
    $data = array($backup_comment);

    //https://stackoverflow.com/questions/5647461/how-do-i-send-a-post-request-with-php
    // use key 'http' even if you send the request to https://...
    $options = array(
        'http' => array(
            'header' => "Content-type: text/plain",
            'method' => 'POST',
            'content' => $backup_comment,
        ),
    );
    $context = stream_context_create($options);

    if (file_get_contents($url, false, $context) !== false) {
        //API now does this directly itself, so all we have to do is call it and let it do it's thing
        //it still calls this same function
        //        DoJsonBackupToUSB();

        return true;
    } else {
        /* Handle error */
        error_log('GenerateBackupViaAPI: Something went wrong trying to call backups API to make a settings backup. (' . json_encode(['url' => $url, 'options' => $options]));
        return false;
    }
}

/**
 * Copy JSON Settings backups to the specified alternate location if set.
 * @return bool
 */
function DoJsonBackupToUSB()
{
    global $settings;
    //Gather some setting that will assist in making a copy of the backups
    $selected_jsonConfigBackupUSBLocation = $settings['jsonConfigBackupUSBLocation'];
    //Folder under which the backups will be stored on the selected storeage device
    $fileCopy_BackupPath = 'Automatic_Backups';
    $result = false;

    //first check if the jsonConfigBackupUSBLocation setting is valid
    if (
        (isset($selected_jsonConfigBackupUSBLocation) && !empty($selected_jsonConfigBackupUSBLocation) && strtolower($selected_jsonConfigBackupUSBLocation) !== 'none')
        &&
        (isset($fileCopy_BackupPath))
    ) {
        //Make a call to the copystorage.php page (i.e the File Copy Backup page)

        //Build up the URL, include the necessary params so we can call it and have JSON Backups copied across
        $url = 'http://localhost/copystorage.php?wrapped=1&direction=TOUSB&path=' . urlencode($fileCopy_BackupPath) . '&storageLocation=' . $selected_jsonConfigBackupUSBLocation . '&flags=JsonBackups&delete=no';

        if (file_get_contents($url, false) === false) {
            /* Handle error */
            error_log('DoJsonBackupToUSB: Something went wrong trying to call filecopy endpoint to copy JSON Backups to the specified device. (' . json_encode(
                [
                    'url' => $url,
                    'jsonConfigBackupUSBLocation' => $selected_jsonConfigBackupUSBLocation,
                    'fileCopy_BackupPath' => $fileCopy_BackupPath,
                ]
            ));
        } else {
            $result = true;
        }
    } else {
        $result = false;

        /* Handle error */
        error_log('DoJsonBackupToUSB: Something went wrong trying to call filecopy endpoint. jsonConfigBackupUSBLocation not specified or missing fileCopy_BackupPath. (' . json_encode(
            [
                'jsonConfigBackupUSBLocation' => $selected_jsonConfigBackupUSBLocation,
                'fileCopy_BackupPath' => $fileCopy_BackupPath,
            ]
        ));
    }

    return $result;
}

/**
 * Helper function used to generate a nice string about what the setting is, which can be used for the comment of what the backup is
 * @param $setting_name
 * @param $setting_value
 * @return string
 */
function GenerateBackupComment($setting_name, $setting_value)
{
    $backup_comment = "FPP Settings - ";
    //Read in the setting JSON file to assist with generating backup comment in relation to what setting was changed
    $settings_json = json_decode(file_get_contents("./settings.json"), true);
    //Try find  the setting name passed via Params in this array
    if (is_array($settings_json) && array_key_exists($setting_name, $settings_json['settings'])) {
        if (array_key_exists('description', $settings_json['settings'][$setting_name])) {
            $settings_description = $settings_json['settings'][$setting_name]['description'];

            $backup_comment .= $settings_description . " setting was set to ( " . $setting_value . " ).";

        } else {
            $backup_comment .= "Setting " . $setting_name . " was set to (" . $setting_value . ").";
        }
    } else {
        $backup_comment .= "Setting " . $setting_name . " was set to (" . $setting_value . ").";
    }

    return $backup_comment;
}

?>
