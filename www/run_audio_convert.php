<?
/*
 * Convert Rate, on the File Manager's Music tab.
 *
 * Takes a JSON list of music file names in the POST body and writes a copy of
 * each at the audio system's configured sample rate, beside the original, as
 * "<name>_<rate>Hz.<ext>".  The original is never touched and an existing file
 * is never overwritten.
 *
 * Why: audio at any rate other than the one the audio system runs at is
 * resampled on the CPU every time it plays, which is expensive on the smaller
 * boards.  Converting a file once, ahead of time, moves that cost out of the
 * show.  Files are encoded as the same kind of file they came from (MP3 stays
 * MP3, FLAC stays FLAC), and a lossy file is encoded again, so it loses a little
 * quality; the copy is written at the original's bit rate, rounded up to the
 * next standard one.
 *
 * Output streams to the browser as it is produced (like run_mp3gain.php) and is
 * also written to fppd.log.
 */

$skipJSsettings = 1;
require_once("common.php");
require_once("common/metadata.php");
require_once("common/oplog.inc.php");

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
if ($lock === false || !flock($lock, LOCK_EX | LOCK_NB)) {
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
    if ($info['sampleRate'] === $target) {
        convertLog('Skipping ' . $name . ': already at ' . $target . ' Hz');
        $skipped++;
        continue;
    }

    $outName = pathinfo($name, PATHINFO_FILENAME) . '_' . $target . 'Hz.' . $ext;
    $outPath = $musicDir . '/' . $outName;
    if (file_exists($outPath)) {
        convertLog('Skipping ' . $name . ': ' . $outName . ' already exists');
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
        '-map', '0:a:0', '-map', '0:v?', '-c:v', 'copy', '-map_metadata', '0',
        '-af', $filter, '-c:a', $codec);
    $cmd = array_merge($cmd, $bitrateArgs);
    if ($extLower === 'mp3') {
        array_push($cmd, '-id3v2_version', '3');
    }
    $cmd[] = $tmp;

    convertLog('Converting ' . $name . ' (' . $info['sampleRate'] . ' Hz -> ' . $target . ' Hz) as ' . $outName);
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
            $ok = @rename($tmp, $outPath);
            if (!$ok) {
                // The temporary folder may be on another filesystem.
                $ok = @copy($tmp, $outPath);
                @unlink($tmp);
            }
            if (!$ok) {
                convertLog('  FAILED: could not write ' . $outName);
            }
        } else {
            convertLog('  FAILED: the result is at ' . $check['sampleRate'] . ' Hz, not ' . $target . ' Hz');
        }
    } else {
        convertLog('  FAILED: ffmpeg exited with ' . $rc);
    }
    @unlink($tmp);

    if ($ok) {
        convertLog(sprintf('  Done in %.1f s: %s (%s)', microtime(true) - $started, $outName, humanFileSize(filesize($outPath))));
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
