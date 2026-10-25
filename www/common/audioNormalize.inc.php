<?php
// Audio Normalize -- makes songs in the music folder equally loud.
//
// Replaces the File Manager's MP3Gain button. MP3Gain had three problems:
//   * It is broken on upgraded systems (stale binary vs. libmpg123 ABI,
//     "undefined symbol: mpg123_decode_frame") and is unmaintained upstream.
//   * It only handles MP3, adjusts in 1.5 dB steps, and with -k ("don't
//     clip") a loud-peaked song is left quieter than the rest -- so a show
//     still has songs that jump in volume.
//   * ReplayGain 1 is not how people hear loudness on show speakers / FM.
//
// This measures integrated loudness (ITU-R BS.1770 / EBU R128) and true peak
// with ffmpeg's loudnorm filter (measure-only) and applies ONE fixed gain per
// song, so mix and dynamics are untouched. ffmpeg is already installed on
// every FPP image. Two ways to match songs:
//   * 'lufs'     -- every song lands on the target integrated loudness.
//   * 'speakers' -- matches how loud a typical moment is with everything
//                   under 120 Hz removed (median short-term loudness), since
//                   outdoor speakers and FM barely reproduce deep bass. A
//                   bass-heavy song can read "on target" in LUFS yet sound
//                   2-3 dB quieter outside. Offset so a typical song still
//                   lands on the target, and capped at +/-4 dB from plain LUFS.
//
// Per song: measure -> decide gain -> encode (same format, sample rate and
// channel count as the original, tags and cover art kept, so sequences timed
// against it still line up) -> verify (re-measure; decoded length compared;
// a peak overshoot from the MP3 encoder is redone once). If raising a quiet
// song would push its peaks over the ceiling, an optional look-ahead limiter
// running at 4x the sample rate catches just those peaks; without it the song
// is raised only as far as its peaks allow.
//
// The first original of every song is kept in <media>/backups/normalize so a
// run can be undone (mp3gain -u used to be the undo). One song at a time,
// single-threaded ffmpeg at the lowest CPU priority so a running show is
// disturbed as little as possible.

require_once(__DIR__ . "/oplog.inc.php");

define('AN_SKIP_TOLERANCE_LU', 0.5);
define('AN_DURATION_WARN_SEC', 0.05);
define('AN_SPEAKER_OFFSET_DB', 3.0);
define('AN_SPEAKER_MAX_DEV_DB', 4.0);
define('AN_SPEAKER_HIGHPASS', 'highpass=f=120,highpass=f=120');
define('AN_LOUDNORM_MEASURE', 'loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json');

// Output encoding per extension -- same format as the original so the file
// keeps its name (playlists and sequences point at the media file by name).
function AN_Formats()
{
    return array(
        'mp3' => array('codec' => array('-c:a', 'libmp3lame', '-b:a', '320k'), 'encoder' => 'libmp3lame', 'muxer' => 'mp3', 'id3' => true, 'art' => true),
        'm4a' => array('codec' => array('-c:a', 'aac', '-b:a', '256k'), 'encoder' => 'aac', 'muxer' => 'ipod', 'id3' => false, 'art' => false),
        'aac' => array('codec' => array('-c:a', 'aac', '-b:a', '256k'), 'encoder' => 'aac', 'muxer' => 'adts', 'id3' => false, 'art' => false),
        'wav' => array('codec' => array('-c:a', 'pcm_s16le'), 'encoder' => 'pcm_s16le', 'muxer' => 'wav', 'id3' => false, 'art' => false),
        'flac' => array('codec' => array('-c:a', 'flac'), 'encoder' => 'flac', 'muxer' => 'flac', 'id3' => false, 'art' => false),
        'ogg' => array('codec' => array('-c:a', 'libvorbis', '-q:a', '8'), 'encoder' => 'libvorbis', 'muxer' => 'ogg', 'id3' => false, 'art' => false),
    );
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

function AN_Ffprobe()
{
    foreach (array('/usr/bin/ffprobe', '/usr/local/bin/ffprobe', '/opt/homebrew/bin/ffprobe') as $p) {
        if (is_executable($p)) {
            return $p;
        }
    }
    return 'ffprobe';
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
function AN_Run($argv, $timeoutSec = 1800)
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
    $r = array('ok' => false, 'loudnorm' => false, 'alimiter' => false, 'limiterLatency' => false, 'speakers' => false, 'encoders' => array());
    list($rc, $filters) = AN_Run(array(AN_Ffmpeg(), '-hide_banner', '-filters'), 30);
    if ($rc !== 0) {
        return $r;
    }
    $has = function ($name) use ($filters) {
        return preg_match('/\b' . preg_quote($name, '/') . '\b/', $filters) === 1;
    };
    $r['loudnorm'] = $has('loudnorm');
    $r['alimiter'] = $has('alimiter');
    $r['speakers'] = $has('ebur128') && $has('highpass') && $has('asplit') && $has('ametadata') && $has('anullsink');
    if ($r['alimiter']) {
        // ffmpeg 5+: compensate the limiter's look-ahead delay.
        list(, $o, $e) = AN_Run(array(AN_Ffmpeg(), '-hide_banner', '-h', 'filter=alimiter'), 30);
        $r['limiterLatency'] = preg_match('/\blatency\b/', $o . $e) === 1;
    }
    list(, $enc) = AN_Run(array(AN_Ffmpeg(), '-hide_banner', '-encoders'), 30);
    foreach (AN_Formats() as $ext => $f) {
        $r['encoders'][$ext] = preg_match('/\s' . preg_quote($f['encoder'], '/') . '\s/', $enc) === 1;
    }
    list($prc) = AN_Run(array(AN_Ffprobe(), '-hide_banner', '-version'), 30);
    $r['ok'] = $r['loudnorm'] && $prc === 0 && !empty($r['encoders']['mp3']);
    return $r;
}

function AN_Probe($file)
{
    list($rc, $out, $err) = AN_Run(array(AN_Ffprobe(), '-v', 'error', '-print_format', 'json', '-show_format', '-show_streams', $file), 60);
    if ($rc !== 0) {
        throw new Exception('could not read the file (' . AN_LastLine($err) . ')');
    }
    $j = json_decode($out, true);
    if (!is_array($j)) {
        throw new Exception('could not read the file');
    }
    $audio = null;
    $hasArt = false;
    foreach ($j['streams'] ?? array() as $s) {
        if (($s['codec_type'] ?? '') === 'audio' && $audio === null) {
            $audio = $s;
        }
        if (($s['codec_type'] ?? '') === 'video') {
            $hasArt = true;
        }
    }
    if ($audio === null) {
        throw new Exception('no audio in this file');
    }
    return array(
        'sampleRate' => intval($audio['sample_rate'] ?? 0) ?: 44100,
        'channels' => intval($audio['channels'] ?? 0) ?: 2,
        'duration' => floatval($audio['duration'] ?? ($j['format']['duration'] ?? 0)),
        'hasArt' => $hasArt,
    );
}

// loudnorm prints a JSON block at the end of stderr.
function AN_ParseLoudnorm($stderr)
{
    $end = strrpos($stderr, '}');
    if ($end === false) {
        return null;
    }
    $start = strrpos(substr($stderr, 0, $end), '{');
    if ($start === false) {
        return null;
    }
    $j = json_decode(substr($stderr, $start, $end - $start + 1), true);
    return is_array($j) ? $j : null;
}

function AN_Num($v)
{
    return is_numeric($v) ? floatval($v) : null;
}

// Decoded length as a sample count (astats "Number of samples", per channel).
// Header durations are bitrate guesses for VBR MP3s without a Xing header and
// can be off by seconds.
function AN_DecodedSamples($stderr)
{
    $at = strrpos($stderr, 'Overall');
    $s = $at !== false ? substr($stderr, $at) : $stderr;
    return preg_match('/Number of samples:\s*(\d+)/', $s, $m) ? intval($m[1]) : null;
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

function AN_Measure($file, $speakers)
{
    $args = array(AN_Ffmpeg(), '-hide_banner', '-nostats', '-threads', '1', '-i', $file);
    if ($speakers) {
        $args[] = '-filter_complex';
        $args[] = '[0:a:0]asplit=2[m][s];[m]astats,' . AN_LOUDNORM_MEASURE . '[mo];' .
            '[s]' . AN_SPEAKER_HIGHPASS . ',ebur128=metadata=1,ametadata=mode=print:key=lavfi.r128.S:file=-,anullsink';
        $args[] = '-map';
        $args[] = '[mo]';
    } else {
        array_push($args, '-map', '0:a:0', '-af', 'astats,' . AN_LOUDNORM_MEASURE);
    }
    array_push($args, '-f', 'null', '-');
    list($rc, $out, $err) = AN_Run($args);
    if ($rc !== 0) {
        throw new Exception('measuring failed (' . AN_LastLine($err) . ')');
    }
    $j = AN_ParseLoudnorm($err);
    if ($j === null) {
        throw new Exception('measuring failed (no loudness data)');
    }
    $m = array(
        'i' => AN_Num($j['input_i'] ?? null),
        'tp' => AN_Num($j['input_tp'] ?? null),
        'samples' => AN_DecodedSamples($err),
        'spk' => $speakers ? AN_SpeakerLevel($out) : null,
    );
    if ($m['i'] === null || $m['tp'] === null || is_infinite($m['i'])) {
        throw new Exception('silent or too short to measure');
    }
    return $m;
}

function AN_R1($n)
{
    return round($n * 10) / 10;
}

// The integrated loudness this song should end up at.
function AN_SongTarget($m, $opts)
{
    $plain = $opts['target'] - $m['i'];
    $gain = $plain;
    $matched = 'lufs';
    $capped = false;
    if ($opts['match'] === 'speakers' && $m['spk'] !== null) {
        $want = ($opts['target'] - AN_SPEAKER_OFFSET_DB) - $m['spk'];
        $gain = max($plain - AN_SPEAKER_MAX_DEV_DB, min($plain + AN_SPEAKER_MAX_DEV_DB, $want));
        $capped = abs($gain - $want) > 0.05;
        $matched = 'speakers';
    }
    return array('target' => $m['i'] + $gain, 'matched' => $matched, 'capped' => $capped);
}

function AN_Plan($m, $target, $opts, $tools)
{
    $gain = $target - $m['i'];
    if (abs($gain) <= AN_SKIP_TOLERANCE_LU && $m['tp'] <= $opts['peak']) {
        return array('action' => 'none', 'gain' => 0);
    }
    if ($m['tp'] + $gain <= $opts['peak']) {
        return array('action' => 'gain', 'gain' => AN_R1($gain));
    }
    if ($opts['limiter'] && $tools['alimiter']) {
        return array('action' => 'limiter', 'gain' => AN_R1($gain), 'limit' => $opts['peak'] - 0.5);
    }
    // Raise only as far as the peaks allow (may still be a cut if the
    // original already peaks over the ceiling).
    return array('action' => 'peak-limited', 'gain' => AN_R1(min($gain, $opts['peak'] - $m['tp'])));
}

function AN_Filter($p, $info, $tools)
{
    $vol = 'volume=' . sprintf('%.2f', $p['gain']) . 'dB';
    if ($p['action'] !== 'limiter') {
        return $vol;
    }
    // Look-ahead limiter at 4x the sample rate so peaks between samples (true
    // peak) are caught, then back to the original rate. level=false: no auto
    // make-up gain. latency=true removes the look-ahead delay so timing holds.
    $lin = min(1, max(0.0625, pow(10, $p['limit'] / 20)));
    $over = $info['sampleRate'] * 4;
    $lim = 'alimiter=limit=' . sprintf('%.5f', $lin) . ':attack=5:release=50:level=false';
    if ($tools['limiterLatency']) {
        $lim .= ':latency=true';
    }
    return $vol . ',aresample=' . $over . ',' . $lim . ',aresample=' . $info['sampleRate'];
}

function AN_Encode($src, $dst, $ext, $info, $filter)
{
    $f = AN_Formats()[$ext];
    $base = array(AN_Ffmpeg(), '-hide_banner', '-nostats', '-threads', '1', '-y', '-i', $src);
    $tail = array_merge(array('-af', $filter, '-ar', strval($info['sampleRate']), '-ac', strval($info['channels'])),
        $f['codec'], array('-map_metadata', '0'));
    if ($f['id3']) {
        array_push($tail, '-id3v2_version', '3', '-write_xing', '1');
    }
    array_push($tail, '-f', $f['muxer'], $dst);
    $withArt = $f['art'] && $info['hasArt'];
    $maps = $withArt ? array('-map', '0:a:0', '-map', '0:v:0', '-c:v', 'copy', '-disposition:v:0', 'attached_pic') : array('-map', '0:a:0');
    list($rc, , $err) = AN_Run(array_merge($base, $maps, $tail));
    if ($rc !== 0 && $withArt) {
        // Odd cover art can upset the muxer; the audio matters, the art doesn't.
        list($rc, , $err) = AN_Run(array_merge($base, array('-map', '0:a:0'), $tail));
    }
    if ($rc !== 0) {
        throw new Exception('encoding failed (' . AN_LastLine($err) . ')');
    }
}

// ------------------------------------------------------------------
// Originals: the first original of each song is kept so a run can be undone.
// manifest.json maps name -> sha1 of the file we wrote, so normalizing an
// already-normalized song again keeps the true original, while a song that
// was replaced by a new upload gets a fresh backup.
// ------------------------------------------------------------------
function AN_BackupDir()
{
    return GetDirSetting('backups') . '/normalize';
}

function AN_Manifest()
{
    $f = AN_BackupDir() . '/manifest.json';
    $j = is_file($f) ? json_decode(@file_get_contents($f), true) : null;
    return is_array($j) ? $j : array();
}

function AN_SaveManifest($m)
{
    @file_put_contents(AN_BackupDir() . '/manifest.json', json_encode($m, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
}

function AN_BackupPath($name)
{
    return AN_BackupDir() . '/' . $name;
}

function AN_KeepOriginal($path, $name)
{
    $dir = dirname(AN_BackupPath($name));
    if (!is_dir($dir) && !@mkdir($dir, 0775, true)) {
        throw new Exception('could not create ' . $dir);
    }
    $man = AN_Manifest();
    $bak = AN_BackupPath($name);
    $ours = isset($man[$name]) && is_file($bak) && sha1_file($path) === $man[$name];
    if ($ours) {
        return false; // current file is our own output: keep the true original
    }
    if (!@copy($path, $bak)) {
        throw new Exception('could not keep a copy of the original');
    }
    @touch($bak, filemtime($path));
    return true;
}

function AN_Restore($musicDir, $name)
{
    $bak = AN_BackupPath($name);
    if (!is_file($bak)) {
        throw new Exception('no original kept');
    }
    $dst = $musicDir . '/' . $name;
    $tmp = $dst . '.restore.tmp';
    if (!@copy($bak, $tmp) || !@rename($tmp, $dst)) {
        @unlink($tmp);
        throw new Exception('could not put the original back');
    }
    @touch($dst, filemtime($bak));
    @unlink($bak);
    // Drop sub-folders the backup left empty.
    for ($d = dirname($bak); $d !== AN_BackupDir() && strpos($d, AN_BackupDir() . '/') === 0 && @rmdir($d); $d = dirname($d)) {
    }
    $man = AN_Manifest();
    unset($man[$name]);
    AN_SaveManifest($man);
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

// Normalize one song in place. $say($line) reports progress. Returns a short
// result string for the summary.
function AN_Process($musicDir, $name, $opts, $tools, $say)
{
    $ext = strtolower(pathinfo($name, PATHINFO_EXTENSION));
    $formats = AN_Formats();
    if (!isset($formats[$ext])) {
        throw new Exception('.' . $ext . ' files are not supported');
    }
    if (empty($tools['encoders'][$ext])) {
        throw new Exception('this ffmpeg can\'t write .' . $ext . ' files');
    }
    $path = $musicDir . '/' . $name;
    if (!is_file($path)) {
        throw new Exception('file not found');
    }

    $speakers = $opts['match'] === 'speakers' && $tools['speakers'];
    $info = AN_Probe($path);
    $m = AN_Measure($path, $speakers);
    $durBefore = $m['samples'] ? $m['samples'] / $info['sampleRate'] : $info['duration'];
    $st = AN_SongTarget($m, $opts);
    $tgt = $st['target'];
    $say(sprintf('  measured %.1f LUFS, peak %.1f dBTP%s', $m['i'], $m['tp'],
        $m['spk'] !== null ? sprintf(', speaker level %.1f', $m['spk']) : ''));
    if ($opts['match'] === 'speakers' && $st['matched'] !== 'speakers') {
        $say('  too short to judge on speakers -- matched by LUFS');
    }

    $p = AN_Plan($m, $tgt, $opts, $tools);
    if ($p['action'] === 'none') {
        return 'already right';
    }

    $tmpDir = GetDirSetting('tmp') . '/normalize';
    if (!is_dir($tmpDir) && !@mkdir($tmpDir, 0775, true)) {
        throw new Exception('could not create ' . $tmpDir);
    }
    $out = $tmpDir . '/' . getmypid() . '-' . bin2hex(random_bytes(4)) . '.' . $ext;
    try {
        $after = null;
        $outInfo = null;
        for ($attempt = 0; $attempt < 3; $attempt++) {
            AN_Encode($path, $out, $ext, $info, AN_Filter($p, $info, $tools));
            $outInfo = AN_Probe($out);
            $after = AN_Measure($out, $speakers);
            $over = $after['tp'] - $opts['peak'];
            $worse = abs($tgt - $after['i']) > abs($tgt - $m['i']) + 0.2;
            if ($worse && $p['action'] !== 'peak-limited') {
                // Never hand back a song further from the target than it started.
                $p = array('action' => 'peak-limited', 'gain' => AN_R1(min($tgt - $m['i'], $opts['peak'] - 0.5 - $m['tp'])));
                continue;
            }
            // Limiting short peaks also lowers loudness a little; make up the
            // shortfall, never more than 6 dB past the plain gain.
            $short = $tgt - $after['i'];
            if ($p['action'] === 'limiter' && $short > 0.5 && $attempt < 2 && $over <= 0.1) {
                $next = AN_R1(min(AN_R1($tgt - $m['i']) + 6, $p['gain'] + $short));
                if ($next > $p['gain'] + 0.2) {
                    $p['gain'] = $next;
                    continue;
                }
            }
            if ($over > 0.1 && $attempt < 2) {
                // Encoder overshoot: tighten and redo.
                if ($p['action'] === 'limiter') {
                    $p['limit'] -= $over + 0.2;
                } else if ($opts['limiter'] && $tools['alimiter']) {
                    $p = array('action' => 'limiter', 'gain' => $p['gain'], 'limit' => $opts['peak'] - 0.5 - $over);
                } else {
                    $p['gain'] = AN_R1($p['gain'] - $over - 0.2);
                }
                continue;
            }
            break;
        }
        $durAfter = $after['samples'] ? $after['samples'] / $outInfo['sampleRate'] : $outInfo['duration'];
        $delta = $durAfter - $durBefore;
        if (abs($delta) > AN_DURATION_WARN_SEC) {
            throw new Exception(sprintf('length changed by %+d ms -- original left alone', round($delta * 1000)));
        }

        $kept = AN_KeepOriginal($path, $name);
        // Replace atomically: copy next to the original, then rename over it.
        // A song fppd has open keeps playing from the old inode.
        $stage = $path . '.normalize.tmp';
        if (!@copy($out, $stage)) {
            throw new Exception('could not write the new file');
        }
        @chmod($stage, fileperms($path) & 0777);
        if (!@rename($stage, $path)) {
            @unlink($stage);
            throw new Exception('could not replace the original');
        }
        $man = AN_Manifest();
        $man[$name] = sha1_file($path);
        AN_SaveManifest($man);
    } finally {
        @unlink($out);
    }

    $how = array('gain' => '', 'limiter' => ', limiter on peaks', 'peak-limited' => ', held back by peaks')[$p['action']];
    return sprintf('%+.1f dB -> %.1f LUFS, peak %.1f dBTP%s%s', $p['gain'], $after['i'], $after['tp'], $how,
        $kept ? '' : ' (original from the first run kept)');
}
