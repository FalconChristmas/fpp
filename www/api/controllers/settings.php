<?

function ReadSettingsJSON() {
    global $settings;
    $json = file_get_contents($settings['fppDir'] . '/www/settings.json');

    return json_decode($json, true);
}

function GetSetting() {
    global $settings;

    $sInfos = ReadSettingsJSON();
    $settingName = params('SettingName');

    if (isset($sInfos['settings'][$settingName])) {
        $sInfo = $sInfos['settings'][$settingName];
    } else {
        $sInfo = Array();
    }

    if (isset($settings[$settingName]))
        $sInfo['value'] = $settings[$settingName];

    if (isset($sInfo['optionsURL'])) {
        $json = "";
        if (preg_match('/^\//', $sInfo['optionsURL'])) {
            $json = file_get_contents("http://" . $_SERVER['SERVER_ADDR'] . $sInfo['optionsURL']);
        } else {
            $json = file_get_contents($sInfo['optionsURL']);
        }

        $sInfo['options'] = json_decode($json, true);
    }

    return json($sInfo);
}

function PutSetting() {
    $status = Array();

    return json($status);
}

function GetSettings() {
    global $settings;
    return file_get_contents($settings['fppDir'] . '/www/settings.json');
}

/////////////////////////////////////////////////////////////////////////////
function GetTime() {
    $result = Array();
    //$result['time'] = date('D M d H:i:s T Y'); // Apache needs restarting after a timezone change
    $result['time'] = exec('date');
    return json($result);
}

?>
