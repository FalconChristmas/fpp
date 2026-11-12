<?php
require_once __DIR__ . '/../wled_common.inc.php';
send_wled_json([
    'm' => 0,
    // An object keyed by palette id, as WLED gives it.
    'p' => (object) [
        '0' => [[0, 255, 255, 255], [255, 255, 255, 255]],
    ],
]);
