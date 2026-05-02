<?php
require_once __DIR__ . '/../wled_common.inc.php';
// Minimal "all off" frame — we don't currently sample live pixels for /json/live.
send_wled_json(['leds' => [], 'n' => 1]);
