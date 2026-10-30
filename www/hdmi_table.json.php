<?php
$skipJSsettings = 1;
require_once "common.php";

// The Force HDMI Resolution choices, from hdmi_modes.json, which fppd also
// reads (VideoOutputManager.cpp, OfferedHdmiModes()) for its #3135 banner.
$modes = json_decode(@file_get_contents(__DIR__ . "/hdmi_modes.json"), true);
if (!is_array($modes)) {
    $modes = [];
}
$table = array_merge(["Default"], $modes["all"] ?? []);
if ((strpos($settings['SubPlatform'], "Raspberry Pi 4") !== false) || (strpos($settings['SubPlatform'], "Raspberry Pi 5") !== false) || (strpos($settings['SubPlatform'], "Raspberry Pi Compute Module 5") !== false)) {
    $table = array_merge($table, $modes["pi4pi5"] ?? []);
}
echo json_encode($table);
