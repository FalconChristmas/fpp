<?php
/*
 * The Media Files report under Help > Troubleshooting Commands
 * (www/troubleshoot-commands.json).
 *
 * Probes every file in the music, video and image directories with ffprobe and
 * prints what is in them: codec, sample rate, bit rate, channels, resolution,
 * aspect ratio and frame rate.  The point is to see what people actually play,
 * and in particular which audio is not at the audio engine's sample rate: FPP's
 * audio graph runs at one fixed rate (default.clock.rate, 48000 unless the
 * Audio/Video setting says otherwise) and everything at another rate -- 44.1 kHz
 * MP3s being the usual case -- is resampled on the CPU, which is what hurts on
 * the smaller boards.
 *
 *   (no option)  The full report: summary tables, then one line per file.
 *                Stops after DEFAULT_BUDGET seconds so a very large library
 *                cannot outrun the web server, and says so.
 *   --summary    Counts only, no file names, and a short time budget: the form
 *                manual Diagnostic Reports get.  Files are sampled in random
 *                order, so what it covers is representative of the library.
 *   --budget=N   Stop probing after N seconds.
 *   --music=DIR --videos=DIR --images=DIR
 *                Scan these directories instead of the configured ones.
 *
 * Every ffprobe runs at the lowest CPU priority, one at a time, so the scan
 * does not compete with a show that is playing.
 */

if (PHP_SAPI !== 'cli') {
    die('This script is for the command line only.');
}

define('DEFAULT_BUDGET', 600);
define('SUMMARY_BUDGET', 12);
define('PROBE_TIMEOUT', 20);
define('LINE_WIDTH', 158); // troubleshootingHelper.php folds output at 160 columns
define('UNREADABLE_LISTED', 25);

$summaryOnly = false;
$budget = null;
$override = array();
foreach (array_slice($argv, 1) as $arg) {
    if ($arg === '--summary') {
        $summaryOnly = true;
    } else if (preg_match('/^--budget=(\d+)$/', $arg, $m)) {
        $budget = (int) $m[1];
    } else if (preg_match('/^--(music|videos|images)=(.+)$/', $arg, $m)) {
        $override[$m[1]] = rtrim($m[2], '/');
    } else {
        fwrite(STDERR, "Usage: media_report.php [--summary] [--budget=SECONDS] [--music=DIR] [--videos=DIR] [--images=DIR]\n");
        exit(1);
    }
}
if ($budget === null) {
    $budget = $summaryOnly ? SUMMARY_BUDGET : DEFAULT_BUDGET;
}

// FPP's own settings: where the media directories are.  Same bootstrap as
// scripts/plugin_history.php.
$fppDir = dirname(__DIR__);
ob_start();
if (!isset($_SERVER['REQUEST_URI'])) {
    $_SERVER['REQUEST_URI'] = '/cli/media_report';
}
if (!isset($_SERVER['SCRIPT_NAME'])) {
    $_SERVER['SCRIPT_NAME'] = '/cli/media_report';
}
require_once($fppDir . '/www/config.php');
ob_end_clean();

$mediaDir = rtrim($settings['mediaDirectory'], '/');
$dirs = array(
    'music' => isset($override['music']) ? $override['music'] : $mediaDir . '/music',
    'videos' => isset($override['videos']) ? $override['videos'] : $mediaDir . '/videos',
    'images' => isset($override['images']) ? $override['images'] : $mediaDir . '/images',
);

$ffprobe = trim((string) shell_exec('command -v ffprobe 2>/dev/null'));
if ($ffprobe === '') {
    echo "ffprobe is not installed, so the media files cannot be inspected.\n";
    echo "Install it with: sudo apt-get install ffmpeg\n";
    exit(0);
}
// No timeout on macOS without coreutils: probe unlimited there
$timeoutCmd = trim((string) shell_exec('command -v timeout 2>/dev/null'));
$niceCmd = trim((string) shell_exec('command -v nice 2>/dev/null'));

// ---------------------------------------------------------------------------
// The rate the audio graph runs at.  Three places name one and the last file
// loaded wins: 90-fpp.conf ships a default and 95-fpp-alsa-sink.conf is
// generated from the audio setting and overrides it (see
// scripts/pipewire_diagnostics.sh, "Sample rate consistency").
// ---------------------------------------------------------------------------
function engineRate(&$source)
{
    $confd = '/etc/pipewire/pipewire.conf.d';
    foreach (array('95-fpp-alsa-sink.conf', '90-fpp.conf') as $file) {
        $text = @file_get_contents($confd . '/' . $file);
        if ($text !== false && preg_match('/default\.clock\.rate\s*=\s*(\d+)/', $text, $m)) {
            $source = $file;
            return (int) $m[1];
        }
    }
    $source = 'assumed, no PipeWire configuration found';
    return 48000;
}
$engineSource = '';
$engineRate = engineRate($engineSource);

// ---------------------------------------------------------------------------
// Formatting helpers.  Widths are in characters, not bytes, so a non-ASCII file
// name still lines up.
// ---------------------------------------------------------------------------
function w($s)
{
    // Not mb_strwidth(): the mbstring extension is not always installed.  A
    // name that is not valid UTF-8 is counted in bytes.
    $n = @preg_match_all('/./us', $s);
    return $n === false ? strlen($s) : $n;
}
function pad($s, $width, $right = false)
{
    $gap = $width - w($s);
    if ($gap <= 0) {
        return $s;
    }
    return $right ? str_repeat(' ', $gap) . $s : $s . str_repeat(' ', $gap);
}
function clip($s, $width)
{
    if (w($s) <= $width) {
        return $s;
    }
    $chars = @preg_split('//u', $s, -1, PREG_SPLIT_NO_EMPTY);
    if ($chars === false) {
        return substr($s, 0, max(1, $width - 1)) . '~';
    }
    return implode('', array_slice($chars, 0, max(1, $width - 1))) . '…';
}
function humanSize($bytes)
{
    $units = array('B', 'KB', 'MB', 'GB', 'TB');
    $i = 0;
    $v = (float) $bytes;
    while ($v >= 1024 && $i < count($units) - 1) {
        $v /= 1024;
        $i++;
    }
    return ($i === 0 ? (string) (int) $v : sprintf('%.1f', $v)) . ' ' . $units[$i];
}
function humanDuration($sec)
{
    if ($sec === null || $sec <= 0) {
        return '-';
    }
    $sec = (int) round($sec);
    if ($sec >= 3600) {
        return sprintf('%d:%02d:%02d', intdiv($sec, 3600), intdiv($sec % 3600, 60), $sec % 60);
    }
    return sprintf('%d:%02d', intdiv($sec, 60), $sec % 60);
}
function hz($rate)
{
    if (!$rate) {
        return '-';
    }
    return $rate % 1000 === 0 ? ($rate / 1000) . ' kHz' : sprintf('%.1f kHz', $rate / 1000);
}
function kbps($bps)
{
    return $bps ? (string) (int) round($bps / 1000) : '-';
}
function pct($n, $total)
{
    return $total > 0 ? sprintf('%d%%', (int) round(100 * $n / $total)) : '-';
}

// Aspect ratio: ffprobe's display_aspect_ratio when the file says one, else
// worked out from the pixels (and the pixel shape, for anamorphic video), then
// named when it is a common one.
function aspect($w, $h, $dar, $sar)
{
    if (!$w || !$h) {
        return '-';
    }
    $ratio = $w / $h;
    if ($dar && $dar !== 'N/A' && preg_match('/^(\d+):(\d+)$/', $dar, $m) && $m[2] > 0 && $m[1] > 0) {
        $ratio = $m[1] / $m[2];
    } else if ($sar && $sar !== 'N/A' && preg_match('/^(\d+):(\d+)$/', $sar, $m) && $m[2] > 0 && $m[1] > 0) {
        $ratio *= $m[1] / $m[2];
    }
    $known = array('1:1' => 1.0, '5:4' => 1.25, '4:3' => 4 / 3, '3:2' => 1.5, '16:10' => 1.6,
        '16:9' => 16 / 9, '2:1' => 2.0, '21:9' => 21 / 9, '9:16' => 9 / 16, '3:4' => 0.75, '2:3' => 2 / 3);
    foreach ($known as $name => $r) {
        if (abs($ratio - $r) / $r < 0.01) {
            return $name;
        }
    }
    return sprintf('%.2f:1', $ratio);
}
// "aac (LC)", "h264 (High)": the profile says a lot about how hard a file is to decode
function codecLabel($stream)
{
    $name = $stream['codec_name'] ?? '?';
    $profile = $stream['profile'] ?? '';
    return ($profile !== '' && $profile !== 'unknown' && stripos($profile, $name) !== 0) ? $name . ' (' . $profile . ')' : $name;
}
function scanType($stream)
{
    $order = $stream['field_order'] ?? '';
    if (in_array($order, array('tt', 'bb', 'tb', 'bt'), true)) {
        return 'interlaced';
    }
    return $order === 'progressive' ? 'progressive' : 'unknown';
}
function frameRate($stream)
{
    foreach (array('avg_frame_rate', 'r_frame_rate') as $key) {
        if (isset($stream[$key]) && preg_match('/^(\d+)\/(\d+)$/', $stream[$key], $m) && $m[2] > 0 && $m[1] > 0) {
            $fps = $m[1] / $m[2];
            return rtrim(rtrim(sprintf('%.2f', $fps), '0'), '.');
        }
    }
    return '-';
}
function bitDepth($stream)
{
    // A lossy codec decodes to whatever the decoder likes (MP3 and AAC to 32-bit
    // float); that is not a property of the file.
    $lossy = array('mp3', 'mp2', 'aac', 'vorbis', 'opus', 'wmav1', 'wmav2', 'ac3', 'eac3', 'dts', 'amr_nb', 'amr_wb');
    if (in_array($stream['codec_name'] ?? '', $lossy, true)) {
        return 0;
    }
    foreach (array('bits_per_raw_sample', 'bits_per_sample') as $key) {
        if (!empty($stream[$key]) && (int) $stream[$key] > 0) {
            return (int) $stream[$key];
        }
    }
    if (!empty($stream['sample_fmt']) && preg_match('/^(?:u8|s16|s32|s64|flt|dbl)/', $stream['sample_fmt'], $m)) {
        $map = array('u8' => 8, 's16' => 16, 's32' => 32, 's64' => 64, 'flt' => 32, 'dbl' => 64);
        return $map[$m[0]];
    }
    return 0;
}

// ---------------------------------------------------------------------------
// ffprobe, run one file at a time at the lowest priority.
// ---------------------------------------------------------------------------
$entries = 'format=format_name,duration,size,bit_rate'
    . ':stream=index,codec_type,codec_name,profile,bit_rate,sample_rate,channels,bits_per_sample,'
    . 'bits_per_raw_sample,sample_fmt,width,height,sample_aspect_ratio,display_aspect_ratio,'
    . 'avg_frame_rate,r_frame_rate,pix_fmt,duration,nb_frames,profile,field_order'
    . ':stream_disposition=attached_pic';

function probe($path)
{
    global $ffprobe, $timeoutCmd, $niceCmd, $entries;
    $cmd = array();
    if ($niceCmd !== '') {
        array_push($cmd, $niceCmd, '-n', '19');
    }
    if ($timeoutCmd !== '') {
        array_push($cmd, $timeoutCmd, '-k', '2', (string) PROBE_TIMEOUT);
    }
    array_push($cmd, $ffprobe, '-v', 'error', '-print_format', 'json', '-show_entries', $entries, '-i', $path);
    $proc = @proc_open($cmd, array(1 => array('pipe', 'w'), 2 => array('pipe', 'w')), $pipes);
    if (!is_resource($proc)) {
        return array(null, 'could not run ffprobe');
    }
    $out = stream_get_contents($pipes[1]);
    $err = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $rc = proc_close($proc);
    $data = json_decode($out, true);
    if ($rc !== 0 || !is_array($data) || empty($data['streams'])) {
        // ffprobe's own wording, without its internal address tag or the path we gave it
        $err = trim(strtok($err, "\n"));
        $err = trim(str_replace($path, '', preg_replace('/^\[[^\]]*\]\s*/', '', $err)), ': ');
        if ($rc === 124 || $rc === 137) {
            $err = 'timed out';
        }
        return array(null, $err !== '' ? $err : 'no audio or video streams');
    }
    return array($data, '');
}

// One file's facts, whatever it turned out to be.
function classify($path, $dir, $data)
{
    $format = isset($data['format']) ? $data['format'] : array();
    $streams = $data['streams'];
    $video = null;
    $audio = null;
    foreach ($streams as $s) {
        $type = isset($s['codec_type']) ? $s['codec_type'] : '';
        // Cover art in an MP3/M4A is a "video" stream; it does not make the file a video
        $cover = !empty($s['disposition']['attached_pic']);
        if ($type === 'video' && !$cover && $video === null) {
            $video = $s;
        } else if ($type === 'audio' && $audio === null) {
            $audio = $s;
        }
    }
    $name = $format['format_name'] ?? '';
    $duration = isset($format['duration']) && is_numeric($format['duration']) ? (float) $format['duration'] : null;
    $info = array(
        'path' => $path,
        'dir' => $dir,
        'ext' => strtolower(pathinfo($path, PATHINFO_EXTENSION)),
        'size' => isset($format['size']) ? (int) $format['size'] : (int) @filesize($path),
        'duration' => $duration,
        'fbitrate' => isset($format['bit_rate']) ? (int) $format['bit_rate'] : 0,
    );
    if ($video !== null) {
        $frames = isset($video['nb_frames']) && is_numeric($video['nb_frames']) ? (int) $video['nb_frames'] : null;
        $still = (strpos($name, 'image2') !== false) || (substr($name, -5) === '_pipe') || $name === 'gif'
            || (!$audio && ($frames === 1 || ($duration !== null && $duration < 0.1)));
        $info['type'] = $still ? 'image' : 'video';
        $info['vcodec'] = codecLabel($video);
        $info['scan'] = scanType($video);
        $info['width'] = (int) ($video['width'] ?? 0);
        $info['height'] = (int) ($video['height'] ?? 0);
        $info['aspect'] = aspect($info['width'], $info['height'], $video['display_aspect_ratio'] ?? '', $video['sample_aspect_ratio'] ?? '');
        $info['fps'] = $still ? '-' : frameRate($video);
        $info['pixfmt'] = $video['pix_fmt'] ?? '-';
        $info['vbitrate'] = isset($video['bit_rate']) ? (int) $video['bit_rate'] : 0;
    } else if ($audio !== null) {
        $info['type'] = 'audio';
    } else {
        return null;
    }
    if ($audio !== null) {
        $info['acodec'] = codecLabel($audio);
        $info['arate'] = isset($audio['sample_rate']) ? (int) $audio['sample_rate'] : 0;
        $info['channels'] = (int) ($audio['channels'] ?? 0);
        $info['bits'] = bitDepth($audio);
        $info['abitrate'] = isset($audio['bit_rate']) ? (int) $audio['bit_rate'] : 0;
        // A file-level rate stands in when the stream does not carry one (VBR MP3)
        if ($info['abitrate'] === 0 && $info['type'] === 'audio') {
            $info['abitrate'] = $info['fbitrate'];
        }
    }
    if ($info['type'] === 'video' && $info['vbitrate'] === 0 && $info['fbitrate'] > 0) {
        $info['vbitrate'] = max(0, $info['fbitrate'] - ($info['abitrate'] ?? 0));
    }
    return $info;
}

// ---------------------------------------------------------------------------
// Find the files.
// ---------------------------------------------------------------------------
$files = array();
$dirStats = array();
foreach ($dirs as $kind => $dir) {
    $dirStats[$kind] = array('found' => 0, 'bytes' => 0, 'exists' => is_dir($dir));
    if (!is_dir($dir)) {
        continue;
    }
    $it = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::LEAVES_ONLY,
        RecursiveIteratorIterator::CATCH_GET_CHILD
    );
    foreach ($it as $f) {
        if (!$f->isFile() || $f->getFilename()[0] === '.') {
            continue;
        }
        $files[] = array($kind, $f->getPathname(), $dir);
        $dirStats[$kind]['found']++;
        $dirStats[$kind]['bytes'] += $f->getSize();
    }
}
usort($files, function ($a, $b) {
    return strnatcasecmp($a[1], $b[1]);
});
if ($summaryOnly) {
    // A time-limited pass should see a fair slice of the library, not just the
    // files that sort first.
    shuffle($files);
}

// ---------------------------------------------------------------------------
// Probe them, until the time budget runs out.
// ---------------------------------------------------------------------------
$records = array('audio' => array(), 'video' => array(), 'image' => array());
$unreadable = array();
$scanned = 0;
$started = microtime(true);
$cutOff = false;
foreach ($files as $entry) {
    if (microtime(true) - $started >= $budget) {
        $cutOff = true;
        break;
    }
    list($kind, $path, $root) = $entry;
    $scanned++;
    list($data, $why) = probe($path);
    $info = $data === null ? null : classify($path, $kind, $data);
    if ($info === null) {
        $unreadable[] = array(ltrim(substr($path, strlen($root)), '/'), $why !== '' ? $why : 'no audio or video streams');
        continue;
    }
    $info['rel'] = ltrim(substr($path, strlen($root)), '/');
    $records[$info['type']][] = $info;
}
$elapsed = microtime(true) - $started;

// ---------------------------------------------------------------------------
// The report.
// ---------------------------------------------------------------------------
$out = '';
function line($s = '')
{
    global $out;
    $out .= $s . "\n";
}
function heading($s)
{
    line();
    line($s);
    line(str_repeat('-', min(LINE_WIDTH, max(w($s), 20))));
}

// Count how many files have each value, then print the biggest first.  $mark
// returns a note for a value (e.g. that it gets resampled), or ''.
function countTable($title, $values, $mark = null, $order = null)
{
    $counts = array_count_values($values);
    if (!$counts) {
        return;
    }
    if ($order !== null) {
        uksort($counts, function ($a, $b) use ($order) {
            return array_search($a, $order, true) <=> array_search($b, $order, true);
        });
    } else {
        arsort($counts);
    }
    $total = count($values);
    $labelWidth = w($title);
    foreach (array_keys($counts) as $k) {
        $labelWidth = max($labelWidth, w((string) $k));
    }
    line(pad($title, $labelWidth) . '  ' . pad('Files', 6, true) . '  ' . pad('Share', 5, true));
    foreach ($counts as $value => $n) {
        $note = $mark ? $mark((string) $value) : '';
        line(pad((string) $value, $labelWidth) . '  ' . pad((string) $n, 6, true) . '  '
            . pad(pct($n, $total), 5, true) . ($note !== '' ? '   ' . $note : ''));
    }
    line();
}
function rateNote($rate)
{
    global $engineRate;
    return ((int) $rate > 0 && (int) $rate !== $engineRate) ? '<-- resampled to ' . hz($engineRate) : '';
}
function bitrateBucket($bps)
{
    if (!$bps) {
        return 'unknown';
    }
    $k = $bps / 1000;
    foreach (array(64 => '64 kbps or less', 96 => '65-96 kbps', 128 => '97-128 kbps', 160 => '129-160 kbps',
        192 => '161-192 kbps', 256 => '193-256 kbps', 320 => '257-320 kbps', 512 => '321-512 kbps', 1024 => '513-1024 kbps') as $limit => $label) {
        if ($k <= $limit) {
            return $label;
        }
    }
    return 'over 1024 kbps';
}
$bitrateOrder = array('64 kbps or less', '65-96 kbps', '97-128 kbps', '129-160 kbps', '161-192 kbps',
    '193-256 kbps', '257-320 kbps', '321-512 kbps', '513-1024 kbps', 'over 1024 kbps', 'unknown');
function resolutionBucket($r)
{
    return $r['width'] . 'x' . $r['height'];
}
function channelsLabel($n)
{
    $names = array(1 => 'mono', 2 => 'stereo', 6 => '5.1', 8 => '7.1');
    return $n ? $n . ' (' . ($names[$n] ?? $n . ' ch') . ')' : 'unknown';
}

$audio = $records['audio'];
$video = $records['video'];
$image = $records['image'];

line('FPP Media Report');
line(str_repeat('=', 16));
line();
line('Audio engine rate : ' . hz($engineRate) . '  (' . $engineSource . ')');
line('                     Audio at any other rate is resampled on the CPU while it plays.');
foreach ($dirs as $kind => $dir) {
    $st = $dirStats[$kind];
    $where = $summaryOnly ? $kind : $dir;
    line(pad('Scanned ' . $kind, 18) . ': ' . ($st['exists']
        ? $st['found'] . ' file' . ($st['found'] == 1 ? '' : 's') . ', ' . humanSize($st['bytes']) . '  (' . $where . ')'
        : 'folder not found  (' . $where . ')'));
}
$probed = count($audio) + count($video) + count($image);
line(pad('Result', 18) . ': ' . count($audio) . ' audio, ' . count($video) . ' video, ' . count($image) . ' image'
    . (count($unreadable) ? ', ' . count($unreadable) . ' not readable as media' : '')
    . '  (' . sprintf('%.1f', $elapsed) . ' s)');
if ($cutOff) {
    line();
    line('** Stopped after ' . $budget . ' s having looked at ' . $scanned . ' of ' . count($files) . ' files'
        . ($summaryOnly ? ' (a random sample)' : ' so the page does not time out') . '. '
        . ($summaryOnly ? '' : 'The totals below cover only those files. ') . '**');
}
if ($probed === 0 && !$unreadable) {
    if (!$cutOff) {
        line();
        line('No media files were found.');
    }
    echo $out;
    exit(0);
}

// -- Resampling: the question this report exists to answer --------------------
$audioOff = array_filter($audio, function ($r) {
    global $engineRate;
    return $r['arate'] > 0 && $r['arate'] !== $engineRate;
});
$videoOff = array_filter($video, function ($r) {
    global $engineRate;
    return isset($r['arate']) && $r['arate'] > 0 && $r['arate'] !== $engineRate;
});
$offSeconds = 0;
foreach (array_merge($audioOff, $videoOff) as $r) {
    $offSeconds += $r['duration'] ?? 0;
}
heading('Resampling at ' . hz($engineRate));
line('Audio files not at ' . hz($engineRate) . ': ' . count($audioOff) . ' of ' . count($audio) . ' (' . pct(count($audioOff), count($audio)) . ')');
line('Video soundtracks not at ' . hz($engineRate) . ': ' . count($videoOff) . ' of ' . count(array_filter($video, function ($r) {
    return isset($r['arate']);
})) . ' with sound');
if ($offSeconds > 0) {
    line('Total playing time that is resampled: ' . humanDuration($offSeconds));
}
if (count($audioOff) + count($videoOff) === 0) {
    line('Nothing needs resampling.');
}

// -- Audio ---------------------------------------------------------------------
if ($audio) {
    heading('Audio files (' . count($audio) . ')');
    countTable('Codec', array_column($audio, 'acodec'));
    countTable('Sample rate', array_map(function ($r) {
        return $r['arate'] ? (string) $r['arate'] : 'unknown';
    }, $audio), 'rateNote');
    countTable('Channels', array_map(function ($r) {
        return channelsLabel($r['channels']);
    }, $audio));
    countTable('Bit rate', array_map(function ($r) {
        return bitrateBucket($r['abitrate']);
    }, $audio), null, $bitrateOrder);
    $depths = array_filter(array_column($audio, 'bits'));
    countTable('Bit depth', array_map(function ($b) {
        return $b . '-bit';
    }, $depths));
    countTable('File type', array_column($audio, 'ext'));
}

// -- Video ---------------------------------------------------------------------
if ($video) {
    heading('Video files (' . count($video) . ')');
    countTable('Video codec', array_column($video, 'vcodec'));
    countTable('Resolution', array_map('resolutionBucket', $video));
    countTable('Aspect ratio', array_column($video, 'aspect'));
    countTable('Frame rate', array_column($video, 'fps'));
    countTable('Scan type', array_column($video, 'scan'));
    countTable('Pixel format', array_column($video, 'pixfmt'));
    countTable('Video bit rate', array_map(function ($r) {
        return bitrateBucket($r['vbitrate']);
    }, $video), null, $bitrateOrder);
    $withSound = array_filter($video, function ($r) {
        return isset($r['acodec']);
    });
    if ($withSound) {
        countTable('Soundtrack codec', array_column($withSound, 'acodec'));
        countTable('Soundtrack rate', array_map(function ($r) {
            return $r['arate'] ? (string) $r['arate'] : 'unknown';
        }, $withSound), 'rateNote');
    }
    $noSound = count($video) - count($withSound);
    if ($noSound > 0) {
        line($noSound . ' video file' . ($noSound == 1 ? ' has' : 's have') . ' no soundtrack.');
        line();
    }
    countTable('File type', array_column($video, 'ext'));
}

// -- Images --------------------------------------------------------------------
if ($image) {
    heading('Image files (' . count($image) . ')');
    countTable('Format', array_column($image, 'vcodec'));
    countTable('Resolution', array_map('resolutionBucket', $image));
    countTable('Aspect ratio', array_column($image, 'aspect'));
    countTable('File type', array_column($image, 'ext'));
}

// -- One line per file (not in the Diagnostic Report: file names are the user's) -
if (!$summaryOnly) {
    // Print a table whose first column is the file name, shrunk to what is left
    // of the line once the other columns have taken theirs.
    $table = function ($rows, $columns) {
        $widths = array();
        foreach ($columns as $i => $head) {
            $widths[$i] = w($head);
            foreach ($rows as $r) {
                $widths[$i] = max($widths[$i], w($r[$i]));
            }
        }
        $others = 0;
        foreach ($widths as $i => $wd) {
            if ($i > 0) {
                $others += $wd + 2;
            }
        }
        $widths[0] = max(12, min($widths[0], LINE_WIDTH - $others));
        $right = array();
        foreach ($columns as $i => $head) {
            $right[$i] = in_array($head, array('Rate', 'Ch', 'Bits', 'kbps', 'Size', 'Time', 'FPS', 'V kbps', 'A kbps', 'A rate', 'A ch'), true);
        }
        $fmt = function ($cells) use ($widths, $right) {
            $parts = array();
            foreach ($cells as $i => $c) {
                $parts[] = $i === count($cells) - 1 && !$right[$i] ? $c : pad(clip($c, $widths[$i]), $widths[$i], $right[$i]);
            }
            return rtrim(implode('  ', $parts));
        };
        line($fmt($columns));
        line(str_repeat('-', min(LINE_WIDTH, array_sum($widths) + 2 * (count($widths) - 1))));
        foreach ($rows as $r) {
            line($fmt($r));
        }
        line();
    };

    if ($audio) {
        heading('Every audio file');
        $rows = array();
        foreach ($audio as $r) {
            $rows[] = array($r['rel'], strtoupper($r['ext']), $r['acodec'], $r['arate'] ? (string) $r['arate'] : '-',
                $r['channels'] ?: '-', $r['bits'] ?: '-', kbps($r['abitrate']), humanDuration($r['duration']),
                humanSize($r['size']), $r['arate'] > 0 && $r['arate'] !== $engineRate ? 'resampled' : '');
        }
        $table($rows, array('File', 'Type', 'Codec', 'Rate', 'Ch', 'Bits', 'kbps', 'Time', 'Size', 'Note'));
    }
    if ($video) {
        heading('Every video file');
        $rows = array();
        foreach ($video as $r) {
            $rows[] = array($r['rel'], strtoupper($r['ext']), $r['vcodec'], $r['width'] . 'x' . $r['height'], $r['aspect'],
                $r['fps'], $r['pixfmt'], kbps($r['vbitrate']),
                $r['acodec'] ?? '-', isset($r['arate']) && $r['arate'] ? (string) $r['arate'] : '-',
                isset($r['channels']) && $r['channels'] ? (string) $r['channels'] : '-', kbps($r['abitrate'] ?? 0),
                humanDuration($r['duration']), humanSize($r['size']),
                isset($r['arate']) && $r['arate'] > 0 && $r['arate'] !== $engineRate ? 'audio resampled' : '');
        }
        $table($rows, array('File', 'Type', 'Codec', 'WxH', 'Aspect', 'FPS', 'PixFmt', 'V kbps', 'Audio', 'A rate', 'A ch', 'A kbps', 'Time', 'Size', 'Note'));
    }
    if ($image) {
        heading('Every image file');
        $rows = array();
        foreach ($image as $r) {
            $rows[] = array($r['rel'], strtoupper($r['ext']), $r['vcodec'], $r['width'] . 'x' . $r['height'], $r['aspect'],
                $r['pixfmt'], humanSize($r['size']));
        }
        $table($rows, array('File', 'Type', 'Format', 'WxH', 'Aspect', 'PixFmt', 'Size'));
    }
    if ($unreadable) {
        heading('Not readable as media (' . count($unreadable) . ')');
        foreach (array_slice($unreadable, 0, UNREADABLE_LISTED) as $u) {
            line(clip($u[0], 100) . '  --  ' . clip($u[1], 50));
        }
        if (count($unreadable) > UNREADABLE_LISTED) {
            line('... and ' . (count($unreadable) - UNREADABLE_LISTED) . ' more');
        }
    }
} else if ($unreadable) {
    line(count($unreadable) . ' file' . (count($unreadable) == 1 ? ' was' : 's were') . ' not readable as media.');
}

echo $out;
