<?

$skipJSsettings = 1;
require_once("common.php");
require_once("common/oplog.inc.php");
require_once("common/audioNormalize.inc.php");

DisableOutputBuffering();
// Finish the job if the browser goes away: a closed tab or dropped connection
// would otherwise abort part way through.
ignore_user_abort(true);
set_time_limit(0);

// Body: {"files": [...], "action": "normalize"|"remove",
//        "target": -14, "peak": -1.5, "match": "speakers"|"lufs"}
// A bare array of names (the old run_mp3gain.php body) is accepted too.
$req = json_decode(file_get_contents('php://input'), true);
if (is_array($req) && array_keys($req) === range(0, count($req) - 1)) {
    $req = array('files' => $req);
}
if (!is_array($req) || !is_array($req['files'] ?? null) || !count($req['files'])) {
    http_response_code(400);
    print("No files selected\n");
    exit(0);
}
$action = ($req['action'] ?? 'normalize') === 'remove' ? 'remove' : 'normalize';
$target = is_numeric($req['target'] ?? null) ? floatval($req['target']) : -14.0;
$peak = is_numeric($req['peak'] ?? null) ? floatval($req['peak']) : -1.5;
if ($target < -30 || $target > -5 || $peak < -9 || $peak > 0) {
    http_response_code(400);
    print("Target must be -30..-5 LUFS and the peak ceiling -9..0 dBTP\n");
    exit(0);
}
$opts = array(
    'target' => round($target, 1),
    'peak' => round($peak, 1),
    'match' => ($req['match'] ?? 'speakers') === 'lufs' ? 'lufs' : 'speakers',
);

$dir = GetDirSetting("music");

function Say($line)
{
    echo $line . "\n";
    flush();
    FppdLogLine('run_normalize.php', 'MediaOut', $line);
}

// One run at a time: two would fight over the CPU while a show plays, and
// over media_gain.json.
$tmpDir = GetDirSetting('tmp');
@mkdir($tmpDir, 0775, true);
$lock = @fopen($tmpDir . '/normalize.lock', 'c');
if ($lock === false || !flock($lock, LOCK_EX | LOCK_NB)) {
    Say("Another normalize is already running -- try again when it has finished.");
    exit(0);
}

$names = array();
foreach ($req['files'] as $f) {
    $n = AN_SafeName($f);
    if ($n === null) {
        Say("Skipping invalid name: " . $f);
        continue;
    }
    $names[] = $n;
}

$ok = 0;
$failed = 0;
if ($action === 'remove') {
    try {
        $removed = AN_Remove($names);
        foreach ($names as $n) {
            Say($n . (in_array($n, $removed, true) ? ": normalization removed (plays as recorded)" : ": was not normalized"));
        }
        $ok = count($removed);
    } catch (Exception $e) {
        Say("FAILED: " . $e->getMessage());
        $failed = count($names);
    }
} else {
    $tools = AN_CheckTools();
    if (!$tools['ok']) {
        Say("ffmpeg with the ebur128 filter is required (sudo apt-get install ffmpeg).");
        exit(0);
    }
    if ($opts['match'] === 'speakers' && !$tools['speakers']) {
        Say("This ffmpeg can't measure the speaker level -- matching by LUFS instead.");
        $opts['match'] = 'lufs';
    }
    Say(sprintf("Measuring %d file(s): %s, %.1f LUFS, peaks <= %.1f dBTP\n",
        count($names), $opts['match'] === 'speakers' ? 'matched by show speakers' : 'matched by LUFS',
        $opts['target'], $opts['peak']));
    foreach ($names as $i => $n) {
        Say(sprintf("[%d/%d] %s", $i + 1, count($names), $n));
        try {
            Say("  " . AN_Process($dir, $n, $opts, $tools, 'Say'));
            $ok++;
        } catch (Exception $e) {
            Say("  FAILED: " . $e->getMessage());
            $failed++;
        }
    }
    if ($ok) {
        Say("\nThe files were not changed. The volume change is applied when a song plays; it takes effect the next time each song starts.");
    }
}

flock($lock, LOCK_UN);
fclose($lock);
printf("\n%s complete: %d done, %d failed.", $action === 'remove' ? 'Remove' : 'Normalize', $ok, $failed);
?>
