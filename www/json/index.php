<?php
// /json — combined info+state+effects+palettes payload (matches WLED behavior).
require_once __DIR__ . '/wled_common.inc.php';

send_wled_json([
    'state'    => build_wled_state(),
    'info'     => build_wled_info(),
    'effects'  => fpp_overlay_effects(),
    'palettes' => ['Default'],
]);
