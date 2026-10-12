<?php
// Runs a batch of long per-host MultiSync jobs over one streaming response.
// See common/remoteJobs.inc.php for why, the job types and the wire format.
//
// POST, JSON body:
//   {"maxParallel": 0,                 optional; 0 = no cap beyond the
//                                      MultiSyncMaxParallelJobs setting
//    "jobs": [{"id": "<unique>", "type": "upgrade", "ip": "10.0.0.5",
//              "after": "<id>", ...type-specific fields}, ...]}

$skipJSsettings = 1;
require_once "common.php";
require_once "common/remoteJobs.inc.php";

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    header('Allow: POST');
    echo "POST a JSON job list\n";
    exit(0);
}

// A cross-site form or text/plain POST needs no CORS preflight; an
// application/json one does, and this page answers no preflight.  So
// requiring the type keeps other sites from starting jobs here (upgrades,
// OS upgrades, file pushes to a host of their choosing).
if (stripos(isset($_SERVER['CONTENT_TYPE']) ? $_SERVER['CONTENT_TYPE'] : '', 'application/json') !== 0) {
    http_response_code(415);
    echo "Content-Type must be application/json\n";
    exit(0);
}

$request = json_decode(file_get_contents('php://input'), true);
if (!is_array($request) || !isset($request['jobs']) || !is_array($request['jobs']) || !count($request['jobs'])) {
    http_response_code(400);
    echo "Expected a JSON object with a non-empty \"jobs\" array\n";
    exit(0);
}

$types = RemoteJobTypes();
$ids = array();
foreach ($request['jobs'] as $job) {
    $id = is_array($job) && isset($job['id']) ? $job['id'] : null;
    if (!is_string($id) || $id === '' || strlen($id) > 128 || isset($ids[$id])) {
        http_response_code(400);
        echo "Every job needs a unique string \"id\"\n";
        exit(0);
    }
    if (!isset($job['type']) || !is_string($job['type']) || !isset($types[$job['type']])) {
        http_response_code(400);
        echo "Unknown job type for job $id\n";
        exit(0);
    }
    $ids[$id] = true;
}

$maxParallel = isset($request['maxParallel']) ? max(0, intval($request['maxParallel'])) : 0;
// A fleet upgrade rebuilds every remote at once, each pushing a full -j's worth
// of compiles at the shared build helpers (and downloads at the uplink). Before
// these jobs shared one connection the browser's per-host limit capped that at
// about 6; the setting keeps a ceiling (default 10). 0 = no limit.
$cap = isset($settings['MultiSyncMaxParallelJobs']) ? max(0, intval($settings['MultiSyncMaxParallelJobs'])) : 0;
if ($cap > 0 && ($maxParallel == 0 || $maxParallel > $cap)) {
    $maxParallel = $cap;
}

// Every job here changes a remote system; once started, finish it even if the
// page goes away, rather than cutting an upgrade or copy off mid-stream.
ignore_user_abort(true);
set_time_limit(0);
header('Content-Type: application/x-ndjson');
header('Cache-Control: no-store');
DisableOutputBuffering();

$runner = new RemoteJobRunner($maxParallel);
foreach ($request['jobs'] as $job) {
    $id = $job['id'];
    $out = function ($text) use ($runner, $id) {
        $runner->output($id, $text);
    };
    $after = (isset($job['after']) && is_string($job['after']) && isset($ids[$job['after']])) ? $job['after'] : null;
    $runner->add($id, call_user_func($types[$job['type']], $job, $out), $after);
}
$runner->run();
