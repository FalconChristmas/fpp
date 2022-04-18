<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape
function GetCapeInfo()
{
    global $settings;
    if (isset($settings['cape-info'])) {
        return json($settings['cape-info']);
    }

    halt(404, "No Cape!");
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape/options
function GetCapeOptions()
{
    global $settings;
    $js = array();
    $js[] = "--None--";

    $directory = $settings["fppDir"] . "/capes";
    if ($settings['Platform'] == "Raspberry Pi") {
        $directory = $directory . "/pi";
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
        $directory = $directory . "/pb";
    } else {
        $directory = $directory . "/bbb";
    }

    foreach (scandir($directory) as $file) {
        if (strlen($file) > 11 && substr($file, -11) === "-eeprom.bin") { //at least ends in "-eeprom.bin"
            $js[] = substr($file, 0, strlen($file) - 11);
        }
    }

    return json($js);
}
