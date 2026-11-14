<?php
$skipJSsettings = 1;
require_once "common.php";

// The Force HDMI Resolution choices, from hdmi_modes.json, which fppd also
// reads (VideoOutputManager.cpp, OfferedHdmiModes()) for its #3135 banner.
$modes = json_decode(@file_get_contents(__DIR__ . "/hdmi_modes.json"), true);
if (!is_array($modes)) {
    $modes = [];
}
$table = $modes["all"] ?? [];
if ((strpos($settings['SubPlatform'], "Raspberry Pi 4") !== false) || (strpos($settings['SubPlatform'], "Raspberry Pi Compute Module 4") !== false) || (strpos($settings['SubPlatform'], "Raspberry Pi 5") !== false) || (strpos($settings['SubPlatform'], "Raspberry Pi Compute Module 5") !== false)) {
    $table = array_merge($table, $modes["pi4pi5"] ?? []);
}

// Smallest first, by width, height and refresh: the Pi 4/5 modes belong among
// the others, not after them.
$key = function ($m) {
    return preg_match('/^(\d+)x(\d+)\D*@(\d+)/', $m, $p) ? [(int) $p[1], (int) $p[2], (int) $p[3]] : [0, 0, 0];
};
usort($table, function ($a, $b) use ($key) {
    return $key($a) <=> $key($b);
});

array_unshift($table, "Default");

// A saved value that is not in the list (set through the API, or from an older
// list) must still be offered: the settings page otherwise shows the first
// option, "Default", in its place, so the forced mode looks unset and is lost
// as soon as the select is saved.  Both ports share this list.
foreach (["ForceHDMIResolution", "ForceHDMIResolutionPort2"] as $name) {
    $v = $settings[$name] ?? "";
    if ($v !== "" && !in_array($v, $table, true)) {
        $table[] = $v;
    }
}
echo json_encode($table);
