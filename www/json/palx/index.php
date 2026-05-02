<?php
require_once __DIR__ . '/../wled_common.inc.php';
send_wled_json([
    'm' => 0,
    'p' => [
        '0' => [[0, 255, 255, 255], [255, 255, 255, 255]],
    ],
]);
