<?
/*
 * Convert Rate, on the File Manager's Music tab.
 *
 * Takes a JSON list of music file names in the POST body and converts each one
 * that is being resampled while it plays to the audio system's configured sample
 * rate (a file that already plays without conversion is skipped), replacing the
 * file in place
 * under the same name (as MP3Gain does), so playlists and sequences that name it
 * keep working.  There is no backup: keep your originals elsewhere.
 *
 * The original is only replaced once the conversion has finished and the result
 * has been checked to really be at the target rate.  It is written to a
 * temporary file first and moved over the original in one step, so a failed or
 * interrupted conversion leaves the original exactly as it was.
 *
 * Why: audio at any rate other than the one the audio system runs at is
 * resampled on the CPU every time it plays, which is expensive on the smaller
 * boards.  Converting a file once, ahead of time, moves that cost out of the
 * show.  Files are encoded as the same kind of file they came from (MP3 stays
 * MP3, FLAC stays FLAC), and a lossy file is encoded again, so it loses a little
 * quality; it is written at the original's bit rate, rounded up to the next
 * standard one.
 *
 * Output streams to the browser as it is produced (like run_mp3gain.php) and is
 * also written to fppd.log.
 */

$skipJSsettings = 1;
require_once("common.php");
require_once("common/metadata.php");
require_once("common/oplog.inc.php");

// It rewrites files and cannot be undone, so it takes only a POST with a JSON
// body, which is what the File Manager sends.  A GET is something any web page
// can make a browser send; POST alone is not a defence, since a page on another
// site can POST text/plain without a preflight (Apache's cross-site rule in
// etc/apache2.site is what refuses those).  Requiring application/json is a
// second, independent check: another site cannot send it without a CORS
// preflight, and nothing here allows one.
if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    http_response_code(405);
    echo "This must be requested with POST.\n";
    exit;
}
if (stripos($_SERVER['CONTENT_TYPE'] ?? '', 'application/json') !== 0) {
    http_response_code(415);
    echo "The request body must be JSON.\n";
    exit;
}

DisableOutputBuffering();
// A conversion can outlast PHP's limit, and closing the tab must not leave a
// half-written temporary file behind.
set_time_limit(0);
ignore_user_abort(true);

// What each kind of file is encoded as.  null means "the codec it already has"
// (PCM in a WAV, so 24-bit stays 24-bit).
$ENCODERS = array(
    'mp3' => 'libmp3lame',
    'flac' => 'flac',
    'ogg' => 'libvorbis',
    'm4a' => 'aac',
    'aac' => 'aac',
    'wav' => null,
);
$STANDARD_KBPS = array(64, 96, 112, 128, 160, 192, 224, 256, 320);

function convertLog($line)
{
    echo $line . "\n";
    flush();
    FppdLogLine('run_audio_convert.php', 'MediaOut', $line);
}

echo "Convert Sample Rate\n\n";

$files = json_decode(file_get_contents('php://input'), true);
if (!is_array($files) || count($files) == 0) {
    echo "No files were selected.\n";
    exit;
}

$rates = GetAudioRates();
$target = !empty($rates['configured']) ? (int) $rates['configured'] : 0;
// Convert to the configured rate, unless the hardware cannot use it directly
// (it is resampled anyway): then the rate the engine runs at is the one that
// really plays without conversion.
if ($target > 0 && AudioRateStatus($target, $rates) === 'stuck') {
    $target = (int) $rates['engine'];
}
if ($target <= 0) {
    echo "No audio sample rate is configured on this system, so there is nothing to convert to.\n";
    exit;
}

$ffmpeg = MediaProbeFindBinary('ffmpeg');
if ($ffmpeg === null) {
    echo "ffmpeg is not installed, so files cannot be converted.\n";
    echo "Install it with: sudo apt-get install ffmpeg\n";
    exit;
}
$nice = MediaProbeFindBinary('nice');

// soxr is the better resampler; use it where this ffmpeg was built with it.
$buildConf = (string) shell_exec(escapeshellarg($ffmpeg) . ' -hide_banner -buildconf 2>&1');
$filter = 'aresample=' . $target . (strpos($buildConf, '--enable-libsoxr') !== false ? ':resampler=soxr' : '');

$musicDir = rtrim(GetDirSetting("music"), '/');
$tmpDir = rtrim($settings['mediaDirectory'], '/') . '/tmp';
if (!is_dir($tmpDir)) {
    @mkdir($tmpDir, 0775, true);
}

// One conversion at a time: two at once would swamp a small board.
$lock = @fopen($tmpDir . '/audio_convert.lock', 'c');
if ($lock === false) {
    echo "Cannot create the lock file in " . $tmpDir . ", so no conversion was started.\n";
    echo "Check that the folder exists and the web server can write to it.\n";
    exit;
}
if (!flock($lock, LOCK_EX | LOCK_NB)) {
    echo "Another conversion is already running. Try again when it has finished.\n";
    exit;
}

convertLog('Convert Rate START: ' . count($files) . ' file(s) to ' . $target . ' Hz');
$converted = 0;
$skipped = 0;
$failed = 0;
$n = 0;

foreach ($files as $name) {
    $n++;
    $name = is_string($name) ? $name : '';

    // A bare file name in the music folder, nothing else.
    if ($name === '' || $name[0] === '.' || strpos($name, '/') !== false || strpos($name, '\\') !== false || strpos($name, "\0") !== false) {
        convertLog('Skipping an invalid file name');
        $skipped++;
        continue;
    }
    $src = $musicDir . '/' . $name;
    if (!is_file($src)) {
        convertLog('Skipping ' . $name . ': not found');
        $skipped++;
        continue;
    }

    $ext = pathinfo($name, PATHINFO_EXTENSION);
    $extLower = strtolower($ext);
    if (!array_key_exists($extLower, $ENCODERS)) {
        convertLog('Skipping ' . $name . ': .' . $ext . ' files are not converted');
        $skipped++;
        continue;
    }

    $info = AudioInfoFromProbe(ProbeMediaWithFFProbe($src));
    if ($info['sampleRate'] <= 0) {
        convertLog('Skipping ' . $name . ': no audio found in it');
        $skipped++;
        continue;
    }
    // Convert only a file that is resampled while it plays.  A file at the
    // engine's rate is already fine, whatever rate is configured, and converting
    // it would make it resample; one at the configured rate is either free (the
    // card upsamples it) or resampled anyway, and converting would change nothing.
    $status = AudioRateStatus($info['sampleRate'], $rates);
    if ($status !== 'resample') {
        $why = array(
            'ok' => 'already at the audio system\'s rate (' . $info['sampleRate'] . ' Hz)',
            'hardware' => 'at the configured rate, and the audio hardware upsamples it at no CPU cost',
            'stuck' => 'at the configured rate, but the audio hardware cannot use it, so converting would change nothing',
        );
        convertLog('Skipping ' . $name . ': ' . (isset($why[$status]) ? $why[$status] : 'its rate is not known'));
        $skipped++;
        continue;
    }

    // Same codec family as the source.  A lossy file keeps its bit rate, rounded
    // up to a standard one (192 kbps when the source does not say).
    $codec = $ENCODERS[$extLower];
    if ($codec === null) {
        $codec = (strpos($info['codec'], 'pcm_') === 0) ? $info['codec'] : 'pcm_s16le';
    }
    $bitrateArgs = array();
    if (in_array($codec, array('libmp3lame', 'libvorbis', 'aac'), true)) {
        $kbps = 192;
        if ($info['bitRate'] > 0) {
            $kbps = 320;
            foreach ($STANDARD_KBPS as $std) {
                if ($std * 1000 >= $info['bitRate'] * 0.99) {
                    $kbps = $std;
                    break;
                }
            }
        }
        $bitrateArgs = array('-b:a', $kbps . 'k');
    }

    $tmp = $tmpDir . '/convert_' . getmypid() . '_' . $n . '.' . $extLower;
    $cmd = array();
    if ($nice !== null) {
        array_push($cmd, $nice, '-n', '19');
    }
    array_push($cmd, $ffmpeg, '-hide_banner', '-loglevel', 'warning', '-nostdin', '-n', '-i', $src,
        '-map', '0:a:0');
    // Carry an embedded cover picture across, but only into containers that can
    // hold one: ffmpeg refuses a picture stream in a WAV or a raw AAC file, which
    // would fail the whole conversion.
    if (in_array($extLower, array('mp3', 'flac', 'm4a'), true)) {
        array_push($cmd, '-map', '0:v?', '-c:v', 'copy');
    }
    array_push($cmd, '-map_metadata', '0', '-af', $filter, '-c:a', $codec);
    $cmd = array_merge($cmd, $bitrateArgs);
    if ($extLower === 'mp3') {
        array_push($cmd, '-id3v2_version', '3');
    }
    $cmd[] = $tmp;

    convertLog('Converting ' . $name . ' (' . $info['sampleRate'] . ' Hz -> ' . $target . ' Hz)');
    $started = microtime(true);
    $proc = @proc_open($cmd, array(1 => array('pipe', 'w'), 2 => array('redirect', 1)), $pipes);
    if (!is_resource($proc)) {
        convertLog('  FAILED: could not run ffmpeg');
        $failed++;
        continue;
    }
    while (($line = fgets($pipes[1])) !== false) {
        convertLog('  ' . rtrim($line, "\n"));
    }
    fclose($pipes[1]);
    $rc = proc_close($proc);

    $ok = false;
    if ($rc === 0 && is_file($tmp) && filesize($tmp) > 0) {
        // Trust the result only if it really is at the target rate.
        $check = AudioInfoFromProbe(ProbeMediaWithFFProbe($tmp));
        if ($check['sampleRate'] === $target) {
            // Keep the original's permissions, then move the result over it in
            // one step.  The temporary folder is normally on the same
            // filesystem as the music folder; if it is not, stage the copy
            // beside the original first so the final move is still one step.
            @chmod($tmp, fileperms($src) & 0777);
            $ok = @rename($tmp, $src);
            if (!$ok) {
                $stage = $musicDir . '/.convert_' . getmypid() . '_' . $n . '.' . $extLower;
                if (@copy($tmp, $stage)) {
                    @chmod($stage, fileperms($src) & 0777);
                    $ok = @rename($stage, $src);
                }
                @unlink($stage);
            }
            if ($ok) {
                // The size changed, so the cached values for this file are stale.
                media_audio_info_cache($name, $check, real_filesize($src));
            } else {
                convertLog('  FAILED: could not replace ' . $name . ' (the original is unchanged)');
            }
        } else {
            convertLog('  FAILED: the result is at ' . $check['sampleRate'] . ' Hz, not ' . $target . ' Hz (the original is unchanged)');
        }
    } else {
        convertLog('  FAILED: ' . ($rc === 0 ? 'ffmpeg produced no output' : 'ffmpeg exited with ' . $rc) . ' (the original is unchanged)');
    }
    @unlink($tmp);

    if ($ok) {
        convertLog(sprintf('  Done in %.1f s: %s is now %s Hz (%s)', microtime(true) - $started, $name, $target, humanFileSize(filesize($src))));
        $converted++;
    } else {
        $failed++;
    }
}

convertLog('Convert Rate FINISH: ' . $converted . ' converted, ' . $skipped . ' skipped, ' . $failed . ' failed');
printf("\nConvert Rate Complete...");
flock($lock, LOCK_UN);
fclose($lock);
?>
