<?php
// Long-running per-host MultiSync jobs (FPP upgrade, OS upgrade, file copies,
// branch changes), run side by side inside ONE request.
//
// Why: over plain http a browser holds at most 6 connections per host, and that
// pool is shared by every tab on the host. One long streaming request per
// remote system used all 6 for the whole of a fleet upgrade, so the page's own
// status polls -- and any other tab open on this FPP -- queued until an upgrade
// finished. Here the browser sends one request listing every job; this side
// runs them concurrently and multiplexes their output onto that one response.
//
// Wire format (application/x-ndjson), one JSON object per line:
//   {"id":"<job id>","out":"<text>"}         output from that job
//   {"id":"<job id>","done":true,"ok":bool}  the job finished
//   {"end":true}                             every job has finished
//
// A job type is a generator function (array $job, callable $out) registered in
// RemoteJobTypes(). It yields steps -- RemoteJobHttp, RemoteJobCommand,
// RemoteJobSleep -- and gets the finished step back as the value of the yield,
// so a job reads as straight-line code while the runner keeps every job's
// steps moving in a single curl_multi / stream_select loop. It returns true on
// success. Sub-steps compose with "yield from".

/** An HTTP request. The runner fills in the result fields. */
class RemoteJobHttp
{
    public $url;
    public $connectTimeoutMs = 3000;
    public $timeout = 0;        // whole-transfer limit in seconds, 0 = none
    public $postFields = null;  // array => multipart POST (CURLFile for uploads), '' => empty POST
    public $stream = true;      // true: body goes to the job's output; false: to $body
    public $progress = null;    // callable(dlTotal, dlNow, ulTotal, ulNow)
    public $onData = null;      // callable(string), also given each chunk of the body as it arrives

    public $httpCode = 0;       // 0 = no response at all
    public $errno = 0;          // CURLE_* of the transfer
    public $error = '';
    public $body = '';

    public function __construct($url)
    {
        $this->url = $url;
    }
}

/** A shell command; stdout and stderr go to the job's output. */
class RemoteJobCommand
{
    public $command;
    public $exitCode = -1;

    public function __construct($command)
    {
        $this->command = $command;
    }
}

class RemoteJobSleep
{
    public $seconds;

    public function __construct($seconds)
    {
        $this->seconds = $seconds;
    }
}

class RemoteJobRunner
{
    private $jobs = array();     // id => state
    private $order = array();    // ids, in the order they were added
    private $maxParallel;
    private $mh;
    private $curl = array();     // spl_object_id(handle) => [id, step, handle]
    private $procs = array();    // id => [proc, pipe]
    private $outBuf = array();   // id => text not yet emitted
    private $utf8Carry = array();// id => bytes of a split UTF-8 sequence
    private $lastEmit = 0;

    public function __construct($maxParallel = 0)
    {
        $this->maxParallel = $maxParallel;
        $this->mh = curl_multi_init();
    }

    /**
     * $after names a job that must finish successfully first; if it fails,
     * this one is skipped (and reported as failed).
     */
    public function add($id, Generator $gen, $after = null)
    {
        $this->jobs[$id] = array('gen' => $gen, 'after' => $after, 'state' => 'pending', 'ok' => false, 'wakeAt' => 0, 'queued' => false);
        $this->order[] = $id;
        $this->outBuf[$id] = '';
        $this->utf8Carry[$id] = '';
    }

    public function output($id, $text)
    {
        $this->outBuf[$id] .= $text;
    }

    public function run()
    {
        while ($this->hasUnfinished()) {
            $this->startReady();

            if (count($this->curl)) {
                do {
                    $rc = curl_multi_exec($this->mh, $running);
                } while ($rc == CURLM_CALL_MULTI_PERFORM);
                while (($info = curl_multi_info_read($this->mh)) !== false) {
                    $this->httpDone($info['handle'], $info['result']);
                }
            }

            foreach ($this->order as $id) {
                $job = &$this->jobs[$id];
                if ($job['state'] == 'sleeping' && microtime(true) >= $job['wakeAt']) {
                    $job['state'] = 'running';
                    $this->advance($id, $job['step']);
                }
                unset($job);
            }

            $this->flushOutput();
            if (microtime(true) - $this->lastEmit > 30) {
                // A bare newline, which the client skips. Keeps the response
                // alive under Apache's ProxyTimeout while every job is quiet,
                // e.g. rsync partway through one large video.
                echo "\n";
                flush();
                $this->lastEmit = microtime(true);
            }
            $this->wait();
        }
        $this->flushOutput();
        curl_multi_close($this->mh);
        $this->emit(array('end' => true));
    }

    private function hasUnfinished()
    {
        foreach ($this->jobs as $job) {
            if ($job['state'] != 'done') {
                return true;
            }
        }
        return false;
    }

    private function startReady()
    {
        $active = 0;
        foreach ($this->jobs as $job) {
            if ($job['state'] == 'running' || $job['state'] == 'sleeping') {
                $active++;
            }
        }
        foreach ($this->order as $id) {
            $job = $this->jobs[$id];
            if ($job['state'] != 'pending') {
                continue;
            }
            $after = $job['after'];
            if ($after !== null && isset($this->jobs[$after])) {
                if ($this->jobs[$after]['state'] != 'done') {
                    continue;
                }
                if (!$this->jobs[$after]['ok']) {
                    $this->output($id, "Skipped: a system this one depends on failed.\n");
                    $this->finish($id, false);
                    continue;
                }
            }
            if ($this->maxParallel > 0 && $active >= $this->maxParallel) {
                if (!$job['queued']) {
                    $this->jobs[$id]['queued'] = true;
                    $this->output($id, "Queued: updating " . $this->maxParallel . " systems at a time.\n");
                }
                continue;
            }
            $this->jobs[$id]['state'] = 'running';
            $active++;
            try {
                $this->handleYield($id, $job['gen']->current());
            } catch (Throwable $e) {
                $this->fail($id, $e);
            }
        }

        // Nothing running and nothing could start: the rest wait on each other.
        if ($active == 0) {
            foreach ($this->order as $id) {
                if ($this->jobs[$id]['state'] == 'pending') {
                    $this->output($id, "Skipped: circular dependency between jobs.\n");
                    $this->finish($id, false);
                }
            }
        }
    }

    /** Send a finished step's result back into its job and run to the next step. */
    private function advance($id, $step)
    {
        try {
            $this->handleYield($id, $this->jobs[$id]['gen']->send($step));
        } catch (Throwable $e) {
            $this->fail($id, $e);
        }
    }

    private function handleYield($id, $step)
    {
        $gen = $this->jobs[$id]['gen'];
        if (!$gen->valid()) {
            $this->finish($id, $gen->getReturn() === true);
        } elseif ($step instanceof RemoteJobHttp) {
            $this->startHttp($id, $step);
        } elseif ($step instanceof RemoteJobCommand) {
            $this->startCommand($id, $step);
        } elseif ($step instanceof RemoteJobSleep) {
            $this->jobs[$id]['state'] = 'sleeping';
            $this->jobs[$id]['step'] = $step;
            $this->jobs[$id]['wakeAt'] = microtime(true) + $step->seconds;
        } else {
            throw new LogicException('job yielded something that is not a step');
        }
    }

    private function fail($id, Throwable $e)
    {
        $this->output($id, "\nERROR: " . $e->getMessage() . "\n");
        $this->finish($id, false);
    }

    private function finish($id, $ok)
    {
        $this->jobs[$id]['state'] = 'done';
        $this->jobs[$id]['ok'] = $ok;
        $this->flushOutput($id, true);
        $this->emit(array('id' => $id, 'done' => true, 'ok' => $ok));
    }

    private function startHttp($id, RemoteJobHttp $step)
    {
        $runner = $this;
        $ch = curl_init($step->url);
        // To the addresses the host was judged by, not a fresh lookup.
        if (!RemotePeerPinCurl($ch, $step->url)) {
            curl_close($ch);
            throw new RuntimeException('invalid host');
        }
        // Never follow: a peer could redirect the request back to loopback,
        // which IsRemotePeerHost() keeps out of the URL itself.
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, false);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT_MS, $step->connectTimeoutMs);
        if ($step->timeout > 0) {
            curl_setopt($ch, CURLOPT_TIMEOUT, $step->timeout);
        }
        curl_setopt($ch, CURLOPT_WRITEFUNCTION, function ($ch, $data) use ($runner, $id, $step) {
            if ($step->stream) {
                $runner->output($id, $data);
            } else {
                $step->body .= $data;
            }
            if ($step->onData !== null) {
                ($step->onData)($data);
            }
            return strlen($data);
        });
        if ($step->postFields !== null) {
            curl_setopt($ch, CURLOPT_POST, true);
            curl_setopt($ch, CURLOPT_POSTFIELDS, $step->postFields);
        }
        if ($step->progress !== null) {
            $progress = $step->progress;
            curl_setopt($ch, CURLOPT_NOPROGRESS, false);
            curl_setopt($ch, defined('CURLOPT_XFERINFOFUNCTION') ? CURLOPT_XFERINFOFUNCTION : CURLOPT_PROGRESSFUNCTION, function ($ch, $dlTotal, $dlNow, $ulTotal, $ulNow) use ($progress) {
                $progress($dlTotal, $dlNow, $ulTotal, $ulNow);
                return 0;
            });
        }
        curl_multi_add_handle($this->mh, $ch);
        $this->curl[spl_object_id($ch)] = array($id, $step, $ch);
    }

    private function httpDone($ch, $result)
    {
        $key = spl_object_id($ch);
        list($id, $step) = $this->curl[$key];
        unset($this->curl[$key]);

        $step->errno = $result;
        $step->error = $result ? curl_strerror($result) : '';
        $step->httpCode = curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
        curl_multi_remove_handle($this->mh, $ch);

        $this->advance($id, $step);
    }

    private function startCommand($id, RemoteJobCommand $step)
    {
        $spec = array(0 => array('file', '/dev/null', 'r'), 1 => array('pipe', 'w'), 2 => array('redirect', 1));
        $proc = proc_open($step->command, $spec, $pipes);
        if (!is_resource($proc)) {
            $this->output($id, "ERROR: could not run command\n");
            $this->advance($id, $step);
            return;
        }
        stream_set_blocking($pipes[1], false);
        $this->procs[$id] = array($proc, $pipes[1], $step);
    }

    private function readCommands(array $readable)
    {
        foreach ($this->procs as $id => $p) {
            list($proc, $pipe, $step) = $p;
            if (!in_array($pipe, $readable, true)) {
                continue;
            }
            $data = fread($pipe, 65536);
            if ($data !== false && $data !== '') {
                $this->output($id, $data);
            }
            if (feof($pipe)) {
                fclose($pipe);
                unset($this->procs[$id]);
                $step->exitCode = proc_close($proc);
                $this->advance($id, $step);
            }
        }
    }

    /** Block until something can make progress, or 100ms. */
    private function wait()
    {
        $pipes = array();
        foreach ($this->procs as $p) {
            $pipes[] = $p[1];
        }
        $haveCurl = count($this->curl) > 0;

        if ($haveCurl) {
            if (curl_multi_select($this->mh, count($pipes) ? 0.05 : 0.1) == -1) {
                usleep(20000);
            }
        }
        if (count($pipes)) {
            $read = $pipes;
            $write = null;
            $except = null;
            if (@stream_select($read, $write, $except, 0, $haveCurl ? 0 : 100000) > 0) {
                $this->readCommands($read);
            }
        } elseif (!$haveCurl) {
            usleep(100000); // only sleeping jobs (or none) left
        }
    }

    /**
     * Emit buffered output, one event per job per loop pass. A multi-byte
     * UTF-8 character split across reads is held back until it is whole, so
     * json_encode never sees half a character.
     */
    private function flushOutput($onlyId = null, $final = false)
    {
        foreach ($this->outBuf as $id => $text) {
            if ($onlyId !== null && $id !== $onlyId) {
                continue;
            }
            $text = $this->utf8Carry[$id] . $text;
            $this->outBuf[$id] = '';
            $this->utf8Carry[$id] = '';
            if (!$final) {
                $cut = RemoteJobUtf8CompleteLength($text);
                $this->utf8Carry[$id] = substr($text, $cut);
                $text = substr($text, 0, $cut);
            }
            if ($text !== '') {
                $this->emit(array('id' => $id, 'out' => $text));
            }
        }
    }

    private function emit(array $event)
    {
        echo json_encode($event, JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE) . "\n";
        flush();
        $this->lastEmit = microtime(true);
    }
}

/** Length of $s up to (not including) a trailing incomplete UTF-8 sequence. */
function RemoteJobUtf8CompleteLength($s)
{
    $len = strlen($s);
    for ($i = 1; $i <= 3 && $i <= $len; $i++) {
        $c = ord($s[$len - $i]);
        if (($c & 0xC0) == 0x80) {
            continue; // continuation byte, keep looking for the lead byte
        }
        $need = ($c >= 0xF0) ? 4 : (($c >= 0xE0) ? 3 : (($c >= 0xC0) ? 2 : 1));
        return ($need > $i) ? $len - $i : $len;
    }
    return $len;
}

function RemoteJobValidHost($host)
{
    return IsRemotePeerHost($host);
}

function RemoteJobURL($host, $path)
{
    if (filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV6)) {
        $host = '[' . $host . ']';
    }
    return 'http://' . $host . $path;
}

/**
 * A request to one of the remote's API routes that change state
 * (docs/DEPRECATED.md): POST, which FPP 11 will require, then GET when the
 * remote is older than 10.3 and answers the POST 404 or 405.  Not streamed.
 */
function RemoteJobAction($host, $path, $timeout = 0)
{
    foreach (array('', null) as $postFields) {
        $req = new RemoteJobHttp(RemoteJobURL($host, $path));
        $req->postFields = $postFields;
        $req->timeout = $timeout;
        $req->stream = false;
        $req = yield $req;
        if ($req->httpCode != 404 && $req->httpCode != 405) {
            break;
        }
    }
    return $req;
}

/** The job's validated host, followed by its "fallback" host if it has one. */
function RemoteJobHosts(array $job)
{
    $hosts = array();
    foreach (array('ip', 'fallback') as $key) {
        if (isset($job[$key]) && RemoteJobValidHost($job[$key]) && !in_array($job[$key], $hosts, true)) {
            $hosts[] = $job[$key];
        }
    }
    return $hosts;
}

define('REMOTE_JOB_OK', 0);
define('REMOTE_JOB_FAILED', 1);
define('REMOTE_JOB_NO_RESPONSE', 2);

/**
 * Whether a line of a remote upgrade page's output reports that the upgrade
 * failed.  The page itself always answers HTTP 200, so this is the only place
 * the outcome shows.  Matches what every FPP since 9.5 prints, so an older
 * remote is judged correctly too:
 *   "Upgrade FAILED." / "===== Upgrade Failed ====="  manualUpdate.php
 *   "FPP UPGRADE FAILED"                              upgradeOS.php
 *   "Can not access ..., unable to pull git updates"  scripts/git_pull, which
 *       before this version exited 0 there and so ended "Upgrade Complete"
 *       without having updated anything.
 */
function RemoteJobIsFailureLine($line)
{
    $line = trim($line);
    return preg_match('/^(=====\s*)?(FPP\s+)?Upgrade\s+Failed\b/i', $line)
        || strpos($line, 'unable to pull git updates') !== false;
}

/**
 * Stream a remote "wrapped" page (manualUpdate.php, upgradeOS.php) into the
 * job's output, failing the job if the page reports a failure.  A connection
 * that drops after the page started answering is not a failure: an upgrade
 * can restart the remote's web server, and an OS upgrade reboots before it
 * closes the response.
 */
function RemoteJobStreamPage($host, $path, callable $out)
{
    $failed = false;
    $partial = '';
    $req = new RemoteJobHttp(RemoteJobURL($host, $path));
    $req->postFields = ''; // the pages change state; any FPP takes them by POST
    $req->onData = function ($data) use (&$failed, &$partial) {
        $lines = explode("\n", $partial . $data);
        $partial = array_pop($lines);
        foreach ($lines as $line) {
            if (RemoteJobIsFailureLine($line)) {
                $failed = true;
            }
        }
    };
    $r = yield $req;
    if ($partial !== '' && RemoteJobIsFailureLine($partial)) {
        $failed = true;
    }
    if ($r->httpCode == 0) {
        $out("ERROR: no response from $host: {$r->error}\n");
        return REMOTE_JOB_NO_RESPONSE;
    }
    if ($r->httpCode >= 400) {
        $out("\nERROR: $host returned HTTP {$r->httpCode}\n");
        return REMOTE_JOB_FAILED;
    }
    if ($r->errno != 0) {
        $out("\n(The connection to $host closed before it finished reporting: {$r->error})\n");
    }
    if ($failed) {
        $out("\nFAILED on $host, see above\n");
        return REMOTE_JOB_FAILED;
    }
    $out("\nDone\n");
    return REMOTE_JOB_OK;
}

/** Try the page on each of the job's hosts until one answers. */
function RemoteJobStreamPageAnyHost(array $job, $label, $path, callable $out)
{
    $hosts = RemoteJobHosts($job);
    if (!count($hosts)) {
        $out("ERROR: invalid host\n");
        return false;
    }
    foreach ($hosts as $host) {
        $out("$label @ $host\n");
        $rc = yield from RemoteJobStreamPage($host, $path, $out);
        if ($rc != REMOTE_JOB_NO_RESPONSE) {
            return $rc == REMOTE_JOB_OK;
        }
    }
    return false;
}

function RemoteJobUpgrade(array $job, callable $out)
{
    return yield from RemoteJobStreamPageAnyHost($job, 'Upgrading FPP', '/manualUpdate.php?wrapped=1', $out);
}

function RemoteJobUpgradeOS(array $job, callable $out)
{
    $os = isset($job['os']) ? $job['os'] : '';
    if (!is_string($os) || $os === '' || $os !== basename($os)) {
        $out("ERROR: invalid OS upgrade file\n");
        return false;
    }
    return yield from RemoteJobStreamPageAnyHost($job, 'Upgrading OS', '/upgradeOS.php?wrapped=1&os=' . rawurlencode($os), $out);
}

/** The media directories the MultiSync "Copy Files" settings select. */
function MultiSyncCopyDirs()
{
    global $settings;

    $dirs = array();
    if (!isset($settings['MultiSyncCopySequences']) || $settings['MultiSyncCopySequences'] == 1) {
        $dirs[] = 'sequences';
    }
    $optional = array(
        'MultiSyncCopyEffects' => 'effects',
        'MultiSyncCopyVideos' => 'videos',
        'MultiSyncCopyImages' => 'images',
        'MultiSyncCopyEvents' => 'events',
        'MultiSyncCopyScripts' => 'scripts',
        'MultiSyncCopyMusic' => 'music',
    );
    foreach ($optional as $setting => $dir) {
        if (isset($settings[$setting]) && $settings[$setting] == 1) {
            $dirs[] = $dir;
        }
    }
    return $dirs;
}

/** rsync the selected media directories to the remote's rsync daemon. */
function RemoteJobCopyFiles(array $job, callable $out)
{
    global $settings, $fppHome;

    $hosts = RemoteJobHosts($job);
    if (!count($hosts)) {
        $out("ERROR: invalid host\n");
        return false;
    }
    $ip = $hosts[0];

    $dirs = MultiSyncCopyDirs();
    if (!count($dirs)) {
        $out("You do not have any files set to be copied.  Please select which file types you wish to copy to the remote systems.\n");
        return false;
    }

    $out("Syncing files to remote FPP system at $ip\n");
    $ok = true;
    foreach ($dirs as $dir) {
        $out("==================================================================================\n");
        $out("Syncing $dir dir to $ip\n");
        $compress = "";
        if ($dir == "sequences" && isset($settings['CompressMultiSyncTransfers']) && $settings['CompressMultiSyncTransfers'] == "1") {
            $compress = "-z";
        }
        $src = escapeshellarg($fppHome . "/media/" . $dir . "/");
        // rsync resolves a name itself; give it the address that was judged.
        $dest = escapeshellarg(RemotePeerConnectHost($ip) . "::media/" . $dir . "/");
        // --contimeout: a remote that is down or dropping packets would
        // otherwise hold the job for the whole TCP connect timeout (about two
        // minutes on Linux) per directory before rsync gave up.
        $command = "rsync -rtDlv --modify-window=1 --contimeout=15 $compress --stats $src $dest 2>&1";
        $out("Command: $command\n");
        $out("----------------------------------------------------------------------------------\n");
        $r = yield new RemoteJobCommand($command);
        if ($r->exitCode != 0) {
            $out("\nERROR: rsync exited with status {$r->exitCode}\n");
            $ok = false;
        }
        $out("\n");
    }
    $out($ok ? "Sync Complete.\n" : "Sync finished with errors.\n");
    return $ok;
}

/** Upload files from a local media directory to the remote, one after another. */
function RemoteJobPushFiles(array $job, callable $out)
{
    $hosts = RemoteJobHosts($job);
    if (!count($hosts)) {
        $out("ERROR: invalid host\n");
        return false;
    }
    $host = $hosts[0];

    // Media only.  GetDirSetting() also maps config, logs, backups and crashes,
    // whose files (.htpasswd, authorized_keys, backups holding settings) must
    // not be sent to whatever host the request names.
    $dirKey = isset($job['dir']) ? strtolower((string) $job['dir']) : '';
    $mediaDirs = array('uploads', 'sequences', 'music', 'videos', 'images', 'effects', 'scripts', 'playlists');
    $dir = in_array($dirKey, $mediaDirs, true) ? GetDirSetting($dirKey) : "";
    if ($dir == "") {
        $out("ERROR: invalid directory\n");
        return false;
    }
    $files = (isset($job['files']) && is_array($job['files'])) ? $job['files'] : array();

    $ok = true;
    foreach ($files as $name) {
        $filename = sanitizeFilename($name);
        $fullPath = "$dir/$filename";
        if (!file_exists($fullPath)) {
            $out("ERROR: file not found - $fullPath\n");
            $ok = false;
            continue;
        }

        $out("Uploading $fullPath to $host...\n");
        $up = new RemoteJobHttp(RemoteJobURL($host, '/jqupload.php'));
        $up->connectTimeoutMs = 10000;
        $up->timeout = 86400;
        $up->stream = false;
        $up->postFields = array('myfile' => new CURLFile(realpath($fullPath)));
        $nextReport = time() + 1;
        $up->progress = function ($dlTotal, $dlNow, $ulTotal, $ulNow) use (&$nextReport, $out, $filename) {
            if (time() < $nextReport) {
                return;
            }
            $nextReport = time() + 2;
            $out($ulTotal > 0 ? "Progress($filename): " . intval($ulNow / $ulTotal * 100) . "%\n" : "Finding Remote Host\n");
        };
        $up = yield $up;
        if ($up->errno != 0 || $up->httpCode >= 400 || $up->httpCode == 0) {
            $out("ERROR sending $filename: " . ($up->errno ? $up->error : "HTTP {$up->httpCode}") . "\n");
            $ok = false;
            continue;
        }
        $out("Complete:" . $up->body . "\n");

        // Uploads land in the remote's upload dir; this files them where they belong.
        $mv = yield from RemoteJobAction($host, '/api/file/move/' . rawurlencode($filename), 10);
        $status = json_decode($mv->body, true);
        if (is_array($status) && ($status['status'] ?? '') == 'Invalid Directory' && ($status['dir'] ?? '') == 'move') {
            // Older than 10.3: its POST /api/file/:DirName/:Name took the
            // POST as an upload to a directory named "move" and wrote nothing.
            $mv = new RemoteJobHttp(RemoteJobURL($host, '/api/file/move/' . rawurlencode($filename)));
            $mv->timeout = 10;
            $mv->stream = false;
            $mv = yield $mv;
            $status = json_decode($mv->body, true);
        }
        if ($mv->errno != 0 || $mv->httpCode >= 400 || $mv->httpCode == 0) {
            $out("ERROR finalizing copied file on remote\n");
            $ok = false;
        } elseif (is_array($status) && isset($status['status']) && $status['status'] != 'OK') {
            $out("ERROR finalizing copied file on remote: {$status['status']}\n");
            $ok = false;
        } else {
            $out("File Copy Complete\n");
        }
    }
    return $ok;
}

/** Switch the remote to another git branch, then wait for its fppd to come back. */
function RemoteJobChangeBranch(array $job, callable $out)
{
    $hosts = RemoteJobHosts($job);
    if (!count($hosts)) {
        $out("ERROR: invalid host\n");
        return false;
    }
    $ip = $hosts[0];

    // Same rules as changebranch.php on the remote.
    $branch = isset($job['branch']) ? $job['branch'] : '';
    if (!is_string($branch) || !preg_match('/^[A-Za-z0-9_.\/-]+$/', $branch) || strpos($branch, '..') !== false || $branch[0] === '-') {
        $out("ERROR: Invalid branch '$branch'\n");
        return false;
    }
    $remote = isset($job['remote']) && is_string($job['remote']) && preg_match('/^[a-zA-Z0-9_-]+$/', $job['remote']) ? $job['remote'] : 'origin';

    $out("Changing branch @$ip to $branch (remote: $remote)\n");

    yield from RemoteJobAction($ip, '/api/system/fppd/stop');

    $r = new RemoteJobHttp(RemoteJobURL($ip, '/changebranch.php?branch=' . urlencode($branch) . '&remote=' . urlencode($remote)));
    $r->postFields = '';
    $r = yield $r;
    if ($r->httpCode == 0) {
        $out("ERROR: no response from $ip: {$r->error}\n");
        return false;
    }
    $ok = true;
    if ($r->httpCode >= 400) {
        $out("\nRemote returned HTTP {$r->httpCode} - check branch name\n");
        $ok = false;
    }

    $out("Waiting for fppd to come back up:\n");
    yield new RemoteJobSleep(2);
    $endTime = time() + 60;
    $count = 0;
    while (time() < $endTime) {
        $out('.');
        if (++$count == 15) {
            yield from RemoteJobAction($ip, '/api/system/fppd/restart');
            $count = 1;
        }
        $status = new RemoteJobHttp(RemoteJobURL($ip, '/api/fppd/status'));
        $status->stream = false;
        $status = yield $status;
        if ($status->errno == 0 && $status->httpCode >= 200 && $status->httpCode < 400) {
            $out("\nFPPD Restarted\n");
            return $ok;
        }
        yield new RemoteJobSleep(1);
    }
    $out("\nError, timed out waiting for fppd to restart  Waited 60 seconds.\n");
    return false;
}

/** Job type => generator function(array $job, callable $out). */
function RemoteJobTypes()
{
    return array(
        'upgrade' => 'RemoteJobUpgrade',
        'upgradeOS' => 'RemoteJobUpgradeOS',
        'copyFiles' => 'RemoteJobCopyFiles',
        'pushFiles' => 'RemoteJobPushFiles',
        'changeBranch' => 'RemoteJobChangeBranch',
    );
}
