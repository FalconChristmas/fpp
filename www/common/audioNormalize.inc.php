<?php
// Audio Normalize -- makes songs in the music folder play equally loud.
//
// Replaces the File Manager's MP3Gain button (broken on upgraded systems,
// MP3-only, 1.5 dB steps, and with -k it leaves loud-peaked songs quieter
// than the rest).
//
// Nothing here rewrites audio. Each song is measured once and its gain is
// stored in <config>/media_gain.json; fppd applies it at playback through the
// GStreamer volume element it already has (GStreamerOut.cpp). So timing,
// quality, file size and format never change, every format GStreamer plays is
// covered, and undo is just removing the entry.
//
// Measuring: integrated loudness and true peak (ITU-R BS.1770 / EBU R128)
// with ffmpeg's ebur128 filter -- the same values loudnorm reports, at about
// a third of the CPU. Two ways to match songs:
//   * 'lufs'     -- every song lands on the target integrated loudness.
//   * 'speakers' -- matches how loud a typical moment is with everything
//                   under 120 Hz removed (median short-term loudness), since
//                   outdoor speakers and FM barely reproduce deep bass: a
//                   bass-heavy song can read "on target" in LUFS yet sound
//                   2-3 dB quieter outside. Offset so a typical song still
//                   lands on the target, capped at +/-4 dB from plain LUFS.
// A song is never raised past the point where its true peak would exceed the
// ceiling (playback has no limiter), so a quiet song with sharp peaks may end
// up a little under the target; with commercial masters peaking near 0 dBTP,
// -18 LUFS (the UI default) is a level nearly every song can reach.
//
// Each entry records the file's size; fppd ignores an entry whose size no
// longer matches, so a song replaced by a new upload plays as-is until it is
// measured again.
//
// Videos: a video's sound plays through the same GStreamer volume element, so
// its audio track is measured like a song. Music entries are keyed by the name
// relative to the music folder; video entries by "videos/" + the name relative
// to the videos folder. (A music sub-folder literally named "videos" holding a
// file with the same name as a video would share a key; the size check means
// the wrong file's gain is never applied -- the other one plays as recorded.)

define('AN_SPEAKER_OFFSET_DB', 3.0);
define('AN_SPEAKER_MAX_DEV_DB', 4.0);
define('AN_SPEAKER_HIGHPASS', 'highpass=f=120,highpass=f=120');
// fppd applies the gain with GStreamer's volume element, whose maximum is 10.0
// (+20 dB); larger values are rejected.
define('AN_MAX_GAIN_DB', 20.0);
define('AN_MIN_GAIN_DB', -30.0);
define('AN_VIDEO_PREFIX', 'videos/');

// media_gain.json key for a name in the Music or Videos tab.
function AN_Key($dir, $name)
{
    return ($dir === 'Videos' ? AN_VIDEO_PREFIX : '') . $name;
}

function AN_Ffmpeg()
{
    foreach (array('/usr/bin/ffmpeg', '/usr/local/bin/ffmpeg', '/opt/homebrew/bin/ffmpeg') as $p) {
        if (is_executable($p)) {
            return $p;
        }
    }
    return 'ffmpeg';
}

function AN_Available()
{
    foreach (array('/usr/bin/ffmpeg', '/usr/local/bin/ffmpeg', '/opt/homebrew/bin/ffmpeg') as $p) {
        if (file_exists($p)) {
            return true;
        }
    }
    return false;
}

// Run a command (argv array, no shell) at the lowest CPU priority.
// Returns array(exitCode, stdout, stderr).
function AN_Run($argv, $timeoutSec = 3600)
{
    if (is_executable('/usr/bin/nice')) {
        $argv = array_merge(array('/usr/bin/nice', '-n', '19'), $argv);
    }
    $spec = array(0 => array('file', '/dev/null', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $proc = @proc_open($argv, $spec, $pipes);
    if (!is_resource($proc)) {
        return array(-1, '', 'could not start ' . $argv[0]);
    }
    stream_set_blocking($pipes[1], false);
    stream_set_blocking($pipes[2], false);
    $out = '';
    $err = '';
    $deadline = time() + $timeoutSec;
    while (true) {
        $r = array($pipes[1], $pipes[2]);
        $w = null;
        $e = null;
        if (@stream_select($r, $w, $e, 1) === false) {
            break;
        }
        foreach ($r as $s) {
            $chunk = fread($s, 65536);
            if ($chunk === false || $chunk === '') {
                continue;
            }
            // stdout carries the speaker meter (~70 bytes per 100 ms): 16 MB
            // covers a 3-hour file.
            if ($s === $pipes[1]) {
                if (strlen($out) < 16000000) {
                    $out .= $chunk;
                }
            } else if (strlen($err) < 2000000) {
                $err .= $chunk;
            }
        }
        if (feof($pipes[1]) && feof($pipes[2])) {
            break;
        }
        if (time() > $deadline) {
            proc_terminate($proc, 9);
            $err .= "\ntimed out";
            break;
        }
    }
    fclose($pipes[1]);
    fclose($pipes[2]);
    $rc = proc_close($proc);
    return array($rc, $out, $err);
}

function AN_LastLine($s)
{
    $lines = array_values(array_filter(preg_split('/\r?\n/', trim((string)$s))));
    $l = count($lines) ? $lines[count($lines) - 1] : 'unknown error';
    return substr($l, 0, 200);
}

function AN_CheckTools()
{
    $r = array('ok' => false, 'speakers' => false);
    list($rc, $filters) = AN_Run(array(AN_Ffmpeg(), '-hide_banner', '-filters'), 30);
    if ($rc !== 0) {
        return $r;
    }
    $has = function ($name) use ($filters) {
        return preg_match('/\b' . preg_quote($name, '/') . '\b/', $filters) === 1;
    };
    $r['ok'] = $has('ebur128');
    $r['speakers'] = $r['ok'] && $has('highpass') && $has('asplit') && $has('ametadata') && $has('anullsink');
    return $r;
}

// The summary ebur128 prints when it finishes. The first line carries the
// filter's instance name ("[Parsed_ebur128_1 @ 0x...] Summary:"), the rest
// are unprefixed; pick the block of the instance we want.
function AN_ParseEbur128Summary($stderr, $instance)
{
    $at = strpos($stderr, '[' . $instance . ' @');
    if ($at === false) {
        return null;
    }
    $block = substr($stderr, $at);
    $next = strpos($block, "\n[", 1);
    if ($next !== false) {
        $block = substr($block, 0, $next);
    }
    if (!preg_match('/^\s*I:\s*(-?[\d.]+|-inf)\s*LUFS/m', $block, $mi)) {
        return null;
    }
    $tp = preg_match('/^\s*Peak:\s*(-?[\d.]+|-inf)\s*dBFS/m', $block, $mp) ? $mp[1] : null;
    return array('i' => is_numeric($mi[1]) ? floatval($mi[1]) : null, 'tp' => is_numeric($tp) ? floatval($tp) : null);
}

// Median short-term loudness of the high-passed signal. Skips the first 3 s
// (window not full yet) and near-silence (< -50), so intros, gaps and fades
// don't drag a song's level down. null under ~5 s of sound (caller uses LUFS).
function AN_SpeakerLevel($stdout)
{
    preg_match_all('/lavfi\.r128\.S=(-?[\d.]+)/', $stdout, $m);
    $vals = array();
    foreach ($m[1] as $i => $v) {
        if ($i < 30) {
            continue;
        }
        $f = floatval($v);
        if ($f > -50) {
            $vals[] = $f;
        }
    }
    if (count($vals) < 50) {
        return null;
    }
    sort($vals);
    $n = count($vals);
    $mid = intdiv($n, 2);
    return ($n % 2) ? $vals[$mid] : ($vals[$mid - 1] + $vals[$mid]) / 2;
}

// One decode pass: integrated loudness + true peak, and (speakers) the
// high-passed short-term meter on a second branch.
function AN_Measure($file, $speakers)
{
    $args = array(AN_Ffmpeg(), '-hide_banner', '-nostats', '-threads', '1', '-i', $file);
    if ($speakers) {
        $args[] = '-filter_complex';
        $args[] = '[0:a:0]asplit=2[m][s];[m]ebur128=peak=true:framelog=verbose[mo];' .
            '[s]' . AN_SPEAKER_HIGHPASS . ',ebur128=metadata=1:framelog=verbose,ametadata=mode=print:key=lavfi.r128.S:file=-,anullsink';
        array_push($args, '-map', '[mo]');
        $instance = 'Parsed_ebur128_1';
    } else {
        array_push($args, '-map', '0:a:0', '-af', 'ebur128=peak=true:framelog=verbose');
        $instance = 'Parsed_ebur128_0';
    }
    array_push($args, '-f', 'null', '-');
    list($rc, $out, $err) = AN_Run($args);
    if ($rc !== 0 && preg_match('/matches no streams|does not contain any stream/i', $err)) {
        throw new AN_NoAudio('no audio track');
    }
    if ($rc !== 0) {
        throw new Exception('measuring failed (' . AN_LastLine($err) . ')');
    }
    $m = AN_ParseEbur128Summary($err, $instance);
    if ($m === null) {
        throw new Exception('measuring failed (no loudness data)');
    }
    if ($m['i'] === null || $m['tp'] === null || $m['i'] < -70) {
        throw new Exception('silent or too short to measure');
    }
    $m['spk'] = $speakers ? AN_SpeakerLevel($out) : null;
    return $m;
}

function AN_R1($n)
{
    return round($n * 10) / 10;
}

// The gain for this song: to the target (or the speaker match), never so far
// up that the true peak would pass the ceiling.
//
// 'reach' is the highest target this song could be normalized to without being
// held back. The gain before the ceiling is the target plus a per-song offset
// (both the LUFS and the speaker gain move one for one with the target), so it
// is the ceiling's headroom less that offset.
function AN_Gain($m, $opts)
{
    $plain = $opts['target'] - $m['i'];
    $gain = $plain;
    $matched = 'lufs';
    if ($opts['match'] === 'speakers' && $m['spk'] !== null) {
        $want = ($opts['target'] - AN_SPEAKER_OFFSET_DB) - $m['spk'];
        $gain = max($plain - AN_SPEAKER_MAX_DEV_DB, min($plain + AN_SPEAKER_MAX_DEV_DB, $want));
        $matched = 'speakers';
    }
    $reach = min($opts['peak'] - $m['tp'], AN_MAX_GAIN_DB) - ($gain - $opts['target']);
    $held = '';
    if ($m['tp'] + $gain > $opts['peak']) {
        $gain = $opts['peak'] - $m['tp'];
        $held = 'held back by peaks';
    }
    if ($gain > AN_MAX_GAIN_DB) {
        $gain = AN_MAX_GAIN_DB;
        $held = 'limited to +' . AN_MAX_GAIN_DB . ' dB';
    }
    $gain = max(AN_MIN_GAIN_DB, $gain);
    return array('gain' => AN_R1($gain), 'matched' => $matched, 'held' => $held, 'reach' => $reach);
}

// ------------------------------------------------------------------
// <config>/media_gain.json: { "<name in music dir>": { gainDb, size, ... } }
// Read by fppd at the start of every song (GStreamerOutput::Start).
// ------------------------------------------------------------------
function AN_GainFile()
{
    return GetDirSetting('config') . '/media_gain.json';
}

function AN_LoadGains()
{
    $f = AN_GainFile();
    $j = is_file($f) ? json_decode(@file_get_contents($f), true) : null;
    return is_array($j) ? $j : array();
}

// Load, change and save media_gain.json under a lock, so a rename or delete in
// the File Manager can't race a Normalize run's write and lose an entry.
// $fn(&$all) returns true if it changed anything. Nothing is created when the
// file doesn't exist yet and $create is false.
function AN_UpdateGains($fn, $create = true)
{
    if (!$create && !is_file(AN_GainFile())) {
        return;
    }
    $tmpDir = GetDirSetting('tmp');
    @mkdir($tmpDir, 0775, true);
    $lock = @fopen($tmpDir . '/media_gain.lock', 'c');
    if ($lock !== false) {
        flock($lock, LOCK_EX);
    }
    try {
        $all = AN_LoadGains();
        if ($fn($all)) {
            AN_SaveGains($all);
        }
    } finally {
        if ($lock !== false) {
            flock($lock, LOCK_UN);
            fclose($lock);
        }
    }
}

function AN_SaveGains($g)
{
    $f = AN_GainFile();
    $tmp = $f . '.tmp';
    ksort($g, SORT_NATURAL | SORT_FLAG_CASE);
    $json = count($g) ? json_encode($g, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) : '{}';
    if (@file_put_contents($tmp, $json . "\n") === false || !@rename($tmp, $f)) {
        @unlink($tmp);
        throw new Exception('could not write ' . $f);
    }
}

// A name as the File Manager sends it: relative to the music folder, no "..",
// backslashes, NULs or leading "/".
function AN_SafeName($name)
{
    $s = trim((string)$name);
    if ($s === '' || strlen($s) > 255 || strpbrk($s, "\\\0") !== false || $s[0] === '/') {
        return null;
    }
    foreach (explode('/', $s) as $p) {
        if ($p === '' || $p === '.' || $p === '..') {
            return null;
        }
    }
    if (preg_match('/[\x00-\x1f]/', $s)) {
        return null;
    }
    return $s;
}

// A video without sound: nothing to even out, so not a failure.
class AN_NoAudio extends Exception
{
}

// Measure one song (or a video's sound) and store its gain under $key.
// $say($line) reports progress.
function AN_Process($baseDir, $name, $key, $opts, $tools, $say)
{
    $path = $baseDir . '/' . $name;
    if (!is_file($path)) {
        throw new Exception('file not found');
    }
    $size = filesize($path);
    $speakers = $opts['match'] === 'speakers' && $tools['speakers'];
    $m = AN_Measure($path, $speakers);
    $say(sprintf('  measured %.1f LUFS, peak %.1f dBTP%s', $m['i'], $m['tp'],
        $m['spk'] !== null ? sprintf(', speaker level %.1f', $m['spk']) : ''));
    $g = AN_Gain($m, $opts);
    if ($opts['match'] === 'speakers' && $g['matched'] !== 'speakers') {
        $say('  too short to judge on speakers -- matched by LUFS');
    }
    clearstatcache(true, $path);
    if (filesize($path) !== $size) {
        throw new Exception('the file changed while it was being measured');
    }

    $entry = array(
        'gainDb' => $g['gain'],
        'size' => $size,
        'lufs' => AN_R1($m['i']),
        'truePeak' => AN_R1($m['tp']),
        'speakerLevel' => $m['spk'] === null ? null : AN_R1($m['spk']),
        'target' => $opts['target'],
        'match' => $g['matched'],
        'measured' => date('c'),
    );
    AN_UpdateGains(function (&$all) use ($key, $entry) {
        $all[$key] = $entry;
        return true;
    });

    return array(
        'text' => sprintf('plays at %+.1f dB -> %.1f LUFS, peak %.1f dBTP%s', $g['gain'], $m['i'] + $g['gain'],
            $m['tp'] + $g['gain'], $g['held'] !== '' ? ' (' . $g['held'] . ')' : ''),
        'held' => $g['held'] !== '',
        'reach' => $g['reach'],
    );
}

function AN_Remove($names)
{
    $removed = array();
    AN_UpdateGains(function (&$all) use ($names, &$removed) {
        foreach ($names as $n) {
            if (isset($all[$n])) {
                unset($all[$n]);
                $removed[] = $n;
            }
        }
        return count($removed) > 0;
    }, false);
    return $removed;
}

// ------------------------------------------------------------------
// Keeping media_gain.json in step with the File Manager's rename, copy and
// delete (api/controllers/files.php). An entry is keyed by the file's name, so
// without this a renamed song played un-normalized and a deleted one left an
// entry behind. Each takes absolute paths and does nothing for a file outside
// the music and videos folders. They never fail the file operation itself.
// ------------------------------------------------------------------

// The media_gain.json key for an absolute path, or null.
function AN_KeyForPath($path)
{
    foreach (array('music' => '', 'videos' => AN_VIDEO_PREFIX) as $dir => $prefix) {
        $base = realpath(GetDirSetting($dir));
        if ($base !== false && strpos($path, $base . '/') === 0) {
            $rel = substr($path, strlen($base) + 1);
            return $rel === '' ? null : $prefix . $rel;
        }
    }
    return null;
}

// $src was renamed (or copied, when $copy) to $dst: its entry goes with it.
// Whatever entry $dst had described the file it replaced, so it goes too.
function AN_FileMoved($src, $dst, $copy = false)
{
    $from = AN_KeyForPath($src);
    $to = AN_KeyForPath($dst);
    if ($from === null && $to === null) {
        return;
    }
    try {
        AN_UpdateGains(function (&$all) use ($from, $to, $copy) {
            $entry = ($from !== null && isset($all[$from])) ? $all[$from] : null;
            $changed = false;
            if ($to !== null && isset($all[$to])) {
                unset($all[$to]);
                $changed = true;
            }
            if ($entry !== null) {
                if (!$copy) {
                    unset($all[$from]);
                }
                if ($to !== null) {
                    $all[$to] = $entry;
                }
                $changed = true;
            }
            return $changed;
        }, false);
    } catch (Exception $e) {
        error_log('media_gain.json not updated for ' . $src . ': ' . $e->getMessage());
    }
}

// $path (a file, or a folder and everything in it) was deleted.
function AN_FileDeleted($path)
{
    $key = AN_KeyForPath($path);
    if ($key === null) {
        return;
    }
    try {
        AN_UpdateGains(function (&$all) use ($key) {
            $changed = false;
            foreach (array_keys($all) as $k) {
                if ($k === $key || strpos($k, $key . '/') === 0) {
                    unset($all[$k]);
                    $changed = true;
                }
            }
            return $changed;
        }, false);
    } catch (Exception $e) {
        error_log('media_gain.json not updated for ' . $path . ': ' . $e->getMessage());
    }
}
