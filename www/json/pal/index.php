<?php
require_once __DIR__ . '/../wled_common.inc.php';
// FPP overlay effects don't currently expose a palette concept;
// return a single placeholder so client UIs don't break.
send_wled_json(['Default']);
