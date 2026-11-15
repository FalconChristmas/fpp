<?php
// Evidence for the media-directory banners in menu.inc, so that a screenshot
// from a player nobody can reach names the exact problem and holds enough to
// find what caused it.  menu.inc loads it on every page for the free-space
// check (one stat -f); everything else only runs when a banner shows.  It
// states what is wrong; it does not suggest fixes.
//
// MediaDirectoryProblem() walks the path one folder at a time from /,
// following symlinks the way the kernel does, and stops at the first step
// that fails: that step is the cause.  Everything after it is evidence: what
// that folder actually contains, what it is mounted from, what /etc/fstab
// expected there, and who the web server runs as.
//
// Everything returned is plain text (the caller escapes it).  Paths and file
// contents are shown with every byte outside printable ASCII written as \xNN,
// so a stray CR, BOM, NBSP or look-alike letter is visible in a screenshot.

// Printable-ASCII rendering of $s: other bytes and backslash become \xNN.
function MediaDiagnoseShow($s)
{
    return preg_replace_callback('/[^\x20-\x5b\x5d-\x7e]/', function ($m) {
        return sprintf('\\x%02x', ord($m[0]));
    }, (string) $s);
}

// /etc/fstab and /proc/mounts escape space, tab, newline and backslash as \ooo
function MediaDiagnoseUnescapeMount($s)
{
    return preg_replace_callback('/\\\\([0-7]{3})/', function ($m) {
        return chr(octdec($m[1]));
    }, $s);
}

// Lines of a mount table as arrays of fields, comments and blanks skipped
function MediaDiagnoseMountTable($file)
{
    $rows = array();
    foreach (@file($file) ?: array() as $line) {
        $line = trim($line);
        if ($line == "" || $line[0] == '#') {
            continue;
        }
        $f = preg_split('/\s+/', $line);
        if (count($f) >= 2) {
            $f[1] = MediaDiagnoseUnescapeMount($f[1]);
            $rows[] = $f;
        }
    }
    return $rows;
}

function MediaDiagnoseUserName($uid)
{
    $pw = function_exists('posix_getpwuid') ? posix_getpwuid($uid) : false;
    return $pw !== false ? $pw['name'] : "uid $uid";
}

function MediaDiagnoseGroupName($gid)
{
    $gr = function_exists('posix_getgrgid') ? posix_getgrgid($gid) : false;
    return $gr !== false ? $gr['name'] : "gid $gid";
}

// "path (owner:group mode)"
function MediaDiagnoseDescribe($path)
{
    $st = @lstat($path);
    if ($st === false) {
        return MediaDiagnoseShow($path);
    }
    return MediaDiagnoseShow($path) . " (" . MediaDiagnoseUserName($st['uid']) . ":" .
        MediaDiagnoseGroupName($st['gid']) . " " . sprintf("%04o", $st['mode'] & 07777) . ")";
}

// Who the web server is, and anything in PHP's own setup that limits what it
// can see
function MediaDiagnoseWho()
{
    if (!function_exists('posix_geteuid')) {
        $who = "web server runs as " . get_current_user() . " (PHP has no posix extension, so its uid is unknown)";
    } else {
        $groups = array();
        foreach (posix_getgroups() ?: array() as $gid) {
            $groups[] = MediaDiagnoseGroupName($gid);
        }
        $who = "web server runs as " . MediaDiagnoseUserName(posix_geteuid()) . " (uid " . posix_geteuid() .
            ", groups " . implode(",", array_unique($groups)) . ")";
    }
    $basedir = ini_get('open_basedir');
    if ($basedir != "") {
        $who .= "; PHP open_basedir limits it to " . MediaDiagnoseShow($basedir);
    }
    return $who;
}

// Why looking up $path failed: array(errno, text).  errno 2 is ENOENT.
function MediaDiagnoseLookupError($path)
{
    if (function_exists('posix_access')) {
        if (@posix_access($path, POSIX_F_OK)) {
            return array(0, "");
        }
        $e = posix_get_last_error();
        return array($e, posix_strerror($e));
    }
    error_clear_last();
    $h = @opendir($path);
    if ($h !== false) {
        closedir($h);
        return array(0, "");
    }
    $msg = preg_replace('/^.*?: /', '', error_get_last()['message'] ?? '');
    return array(stripos($msg, "No such file") !== false ? 2 : -1, $msg);
}

// The /proc/mounts row whose mount point holds $path (longest match)
function MediaDiagnoseMountOf($path, $mounts = null)
{
    if ($mounts === null) {
        $mounts = MediaDiagnoseMountTable("/proc/mounts");
    }
    $best = null;
    foreach ($mounts as $f) {
        if (($f[1] == '/' || $path == $f[1] || str_starts_with($path . '/', rtrim($f[1], '/') . '/')) &&
            ($best === null || strlen($f[1]) >= strlen($best[1]))) {
            $best = $f;
        }
    }
    return $best;
}

// "X is on /dev/sda1 mounted at /home/fpp/media (vfat, READ-ONLY)"
function MediaDiagnoseMountLine($path, $mounts = null)
{
    $m = MediaDiagnoseMountOf($path, $mounts);
    if ($m === null) {
        return "";
    }
    $ro = in_array("ro", explode(",", $m[3] ?? ""));
    return MediaDiagnoseShow($path) . " is on " . MediaDiagnoseShow($m[0]) . " mounted at " . MediaDiagnoseShow($m[1]) .
        " (" . ($m[2] ?? "?") . ($ro ? ", READ-ONLY" : "") . ")";
}

// Sizes of the filesystem holding $path: array(totalBytes, availBytes,
// totalInodes, freeInodes), or a string with the error.  Inodes are -1 where
// they cannot be read (macOS) and 0 where the filesystem has no fixed count
// (btrfs).
function MediaDiagnoseStatfs($path)
{
    if (PHP_OS_FAMILY == "Linux") {
        $out = array();
        @exec("stat -f -c '%b %a %S %c %d' " . escapeshellarg($path) . " 2>&1", $out, $rc);
        $f = explode(" ", trim($out[0] ?? ""));
        if ($rc != 0 || count($f) != 5) {
            return trim(preg_replace('/^stat: /', '', implode(" ", $out))) ?: "stat failed";
        }
        return array($f[0] * $f[2], $f[1] * $f[2], (int) $f[3], (int) $f[4]);
    }
    error_clear_last();
    $total = @disk_total_space($path);
    $avail = @disk_free_space($path);
    if ($total === false || $avail === false) {
        return preg_replace('/^.*?: /', '', error_get_last()['message'] ?? 'unknown error');
    }
    return array($total, $avail, -1, -1);
}

function MediaDiagnoseBytes($n)
{
    foreach (array("GB" => 1e9, "MB" => 1e6, "KB" => 1e3) as $u => $d) {
        if ($n >= $d) {
            return sprintf("%.1f %s", $n / $d, $u);
        }
    }
    return "$n bytes";
}

// "45.2 MB free of 3.6 GB, 1200 of 230000 inodes free"
function MediaDiagnoseSpaceLine($path)
{
    $s = MediaDiagnoseStatfs($path);
    if (!is_array($s)) {
        return "the free space on " . MediaDiagnoseShow($path) . " cannot be read: " . $s;
    }
    $line = MediaDiagnoseBytes($s[1]) . " free of " . MediaDiagnoseBytes($s[0]);
    if ($s[2] > 0) {
        $line .= ", " . $s[3] . " of " . $s[2] . " inodes free";
    }
    return $line;
}

// Space on the disk holding partition $dev that no partition uses
function MediaDiagnoseUnpartitioned($dev)
{
    if (PHP_OS_FAMILY != "Linux" || !str_starts_with($dev, "/dev/")) {
        return "";
    }
    $out = array();
    @exec("lsblk -bndo PKNAME " . escapeshellarg($dev) . " 2>/dev/null", $out);
    $disk = trim($out[0] ?? "");
    if ($disk == "") {
        return "";
    }
    $out = array();
    @exec("lsblk -bnro NAME,SIZE,TYPE " . escapeshellarg("/dev/" . $disk) . " 2>/dev/null", $out);
    $size = 0;
    $used = 0;
    foreach ($out as $l) {
        $f = explode(" ", $l);
        if (count($f) < 3) {
            continue;
        }
        if ($f[2] == "disk") {
            $size = (float) $f[1];
        } else if ($f[2] == "part") {
            $used += (float) $f[1];
        }
    }
    if ($size <= 0) {
        return "";
    }
    $line = MediaDiagnoseShow($dev) . " is on /dev/" . MediaDiagnoseShow($disk) . ", a " . MediaDiagnoseBytes($size) . " disk";
    if ($size - $used > 100e6) {
        $line .= " with " . MediaDiagnoseBytes($size - $used) . " not in any partition";
    }
    return $line;
}

// Entries of $folder whose name nearly matches $name: same apart from case,
// stray whitespace or non-ASCII bytes, or one or two letters off.
function MediaDiagnoseNearMatches($folder, $name)
{
    $entries = @scandir($folder);
    if ($entries === false) {
        return false;
    }
    $plain = function ($s) {
        return strtolower(trim(preg_replace('/[^\x21-\x7e]/', '', $s)));
    };
    $want = $plain($name);
    $near = array();
    $all = array();
    foreach ($entries as $e) {
        if ($e == "." || $e == "..") {
            continue;
        }
        $all[] = $e;
        // one letter off for short names, two for longer ones
        if ($plain($e) == $want || ($want != "" && levenshtein($plain($e), $want) <= (strlen($want) >= 6 ? 2 : 1))) {
            $near[] = $e;
        }
    }
    return array("near" => $near, "all" => $all);
}

// What the folder the walk stopped in holds, for "X does not exist"
function MediaDiagnoseFolderContents($cur, $name)
{
    $m = MediaDiagnoseNearMatches($cur, $name);
    if ($m === false) {
        return "the web server cannot list " . MediaDiagnoseDescribe($cur);
    }
    if (count($m['near'])) {
        return MediaDiagnoseShow($cur) . " has a near match: " . implode(", ", array_map(function ($e) {
            return "\"" . MediaDiagnoseShow($e) . "\"";
        }, $m['near']));
    }
    if (count($m['all']) == 0) {
        return MediaDiagnoseDescribe($cur) . " is empty";
    }
    $shown = array_slice($m['all'], 0, 12);
    return MediaDiagnoseDescribe($cur) . " contains " . implode(", ", array_map('MediaDiagnoseShow', $shown)) .
        (count($m['all']) > count($shown) ? " and " . (count($m['all']) - count($shown)) . " more" : "");
}

// The banner when the media directory does not exist.  $dir is
// $mediaDirectory, $raw is $mediaRootRaw (media_root.txt's bytes, null when
// there is no file, false when it cannot be read) and $mkdirError is
// config.php's $mediaDirectoryMkdirError.  Returns array("cause" => string,
// "details" => list of strings).
function MediaDirectoryProblem($dir, $raw, $mkdirError)
{
    $rootFile = dirname(__DIR__) . "/media_root.txt";
    $fppdNote = (PHP_OS_FAMILY == "Linux") ? "; fppd runs as root and reads it, so fppd and the web pages may be using different media folders" : "";
    $details = array();

    // media_root.txt itself
    if ($raw === false || ($raw === "" && is_dir($rootFile))) {
        return array(
            "cause" => (is_dir($rootFile) ? MediaDiagnoseDescribe($rootFile) . " is a folder, not a file" :
                "the web server cannot read " . MediaDiagnoseDescribe($rootFile)) . $fppdNote,
            "details" => array(MediaDiagnoseWho()),
        );
    }
    if ($raw !== null) {
        $raw = (string) $raw;
        if ($raw === "") {
            return array("cause" => MediaDiagnoseDescribe($rootFile) . " is empty", "details" => array());
        }
        $path = trim($raw);
        if (($raw !== $path && $raw !== $path . "\n") || preg_match('/[^\x20-\x7e]/', $path)) {
            $details[] = "media_root.txt holds \"" . MediaDiagnoseShow(substr($raw, 0, 300)) . "\" (" . strlen($raw) . " bytes" .
                (strlen($raw) > 300 ? ", first 300 shown" : "") . ")";
        }
        if (str_contains($path, "\0")) {
            return array("cause" => "the path in media_root.txt contains a NUL byte", "details" => $details);
        }
        $dir = $path;
    }
    if ($dir == "" || $dir[0] != '/') {
        return array("cause" => "the path \"" . MediaDiagnoseShow($dir) . "\" is not absolute (it does not start with /)", "details" => $details);
    }
    clearstatcache();
    $mounts = MediaDiagnoseMountTable("/proc/mounts");
    $mountPoints = array();
    foreach ($mounts as $f) {
        $mountPoints[rtrim($f[1], '/')] = $f;
    }

    // Walk it
    $todo = array_values(array_filter(explode('/', $dir), 'strlen'));
    $cur = "/";
    $cause = "";
    $rest = null;       // the step that failed and those after it
    $mountFor = null;   // where the mount line should look, if not $cur
    $danglingLink = false;
    $hops = 0;
    $seenLinks = array();
    $links = array();
    while ($cause == "" && count($todo)) {
        $c = array_shift($todo);
        if ($c == ".") {
            continue;
        }
        if ($c == "..") {
            $cur = dirname($cur);
            continue;
        }
        $next = rtrim($cur, '/') . '/' . $c;
        $rest = array_merge(array($c), $todo);
        $st = @lstat($next);
        if ($st === false) {
            list($errno, $err) = MediaDiagnoseLookupError($next);
            if (isset($mountPoints[$next]) && ($mountPoints[$next][2] ?? "") != "autofs") {
                $m = $mountPoints[$next];
                $cause = MediaDiagnoseShow($m[0]) . " (" . ($m[2] ?? "?") . ") is mounted at " . MediaDiagnoseShow($next) .
                    " but is not responding" . ($err != "" ? ": $err" : "");
                $mountFor = $next;
            } else if ($errno == 2 || $errno == 0) {
                $cause = MediaDiagnoseShow($next) . " does not exist";
                $danglingLink = $hops > 0;
                $details[] = MediaDiagnoseFolderContents($cur, $c);
            } else if (PHP_OS_FAMILY == "Darwin" && $errno == 1) {
                $cause = "macOS privacy settings (Files and Folders / Full Disk Access) do not let the web server into " .
                    MediaDiagnoseShow($cur) . ": $err";
            } else {
                $cause = MediaDiagnoseShow($next) . " cannot be looked up: $err";
            }
            break;
        }
        if (($st['mode'] & 0170000) == 0120000) {
            $t = @readlink($next);
            if ($t === false || $t === "") {
                $cause = "the web server cannot read the symlink " . MediaDiagnoseDescribe($next);
                break;
            }
            // The same link with the same path left to walk means a loop
            $key = $next . "\0" . implode('/', $todo);
            if (isset($seenLinks[$key]) || ++$hops > 40) {
                $cause = "the symlinks under " . MediaDiagnoseShow($dir) . " loop back on themselves";
                break;
            }
            $seenLinks[$key] = true;
            $links[] = MediaDiagnoseShow($next) . " is a symlink to " . MediaDiagnoseShow($t);
            if ($t[0] == '/') {
                $cur = "/";
            }
            $todo = array_merge(array_values(array_filter(explode('/', $t), 'strlen')), $todo);
            $rest = null;
            continue;
        }
        if (($st['mode'] & 0170000) != 0040000) {
            $cause = MediaDiagnoseDescribe($next) . " is a file, not a folder";
            break;
        }
        if (!is_executable($next)) {
            $cause = "the web server cannot enter " . MediaDiagnoseDescribe($next);
            $details[] = MediaDiagnoseWho();
            $mountFor = $next;
            break;
        }
        $cur = $next;
        $rest = null;
    }
    if (count($links) > 8) {
        $links = array_merge(array_slice($links, 0, 8), array("and " . (count($links) - 8) . " more symlinks"));
    }
    $details = array_merge($links, $details);
    if ($cause == "") {
        $cause = "walking " . MediaDiagnoseShow($dir) . " just now found nothing wrong; it may have been mounted or created after this page started";
    }
    // Where the walk ended up, e.g. /mnt/usb/media for a link into /mnt/usb
    $wanted = ($rest === null) ? $cur : rtrim($cur, '/') . '/' . implode('/', $rest);

    $line = MediaDiagnoseMountLine($mountFor ?? $cur, $mounts);
    if ($line != "") {
        $details[] = $line;
    }
    // Anything /etc/fstab expected to be mounted along the way that is not.
    // An automount point shows in /proc/mounts as autofs before its drive is
    // mounted, so that does not count.
    $mountedAt = array();
    foreach ($mounts as $f) {
        if (($f[2] ?? "") != "autofs") {
            $mountedAt[] = rtrim($f[1], '/');
        }
    }
    foreach (MediaDiagnoseMountTable("/etc/fstab") as $f) {
        $mp = rtrim($f[1], '/');
        if ($mp == "" || $mp == "none" || !str_starts_with($wanted . '/', $mp . '/') || in_array($mp, $mountedAt)) {
            continue;
        }
        $noauto = in_array("noauto", explode(",", $f[3] ?? ""));
        $details[] = "/etc/fstab mounts " . MediaDiagnoseShow($f[0]) . " at " . MediaDiagnoseShow($mp) .
            " but nothing is mounted there" . ($noauto ? " (it is marked noauto)" : "");
    }
    $s = MediaDiagnoseStatfs($cur);
    if (is_array($s) && ($s[1] < 1000000 || ($s[2] > 0 && $s[3] < 100))) {
        $details[] = MediaDiagnoseShow($cur) . "'s filesystem is full: " . MediaDiagnoseSpaceLine($cur);
    }
    // mkdir on a dangling symlink only ever says "File exists"
    if ($mkdirError != "" && !($danglingLink && str_contains($mkdirError, "File exists"))) {
        $details[] = "creating it: " . $mkdirError;
    }
    return array("cause" => $cause, "details" => $details);
}

// Details for the banner when the media directory exists but the web server
// cannot write to it.
function MediaNotWritableDetails($dir)
{
    $details = array(MediaDiagnoseDescribe($dir), MediaDiagnoseWho());
    $line = MediaDiagnoseMountLine($dir);
    if ($line != "") {
        $details[] = $line;
    }
    return $details;
}

// The free-space banner: array("kind" => "", "low", "inodes" or "unknown",
// "summary" => string, "details" => list of strings).  Run on every page, so
// it costs one stat -f on Linux; the rest only runs when a banner shows.
function MediaSpaceProblem($dir)
{
    $s = MediaDiagnoseStatfs($dir);
    if (!is_array($s)) {
        $kind = "unknown";
        $summary = $s;
    } else if ($s[1] < 50000000) {
        $kind = "low";
        $summary = MediaDiagnoseSpaceLine($dir);
    } else if ($s[2] > 0 && $s[3] < max(1000, $s[2] / 100)) {
        // Every file and folder takes an inode; with none left nothing new can
        // be created, however much space is free.
        $kind = "inodes";
        $summary = MediaDiagnoseSpaceLine($dir);
    } else {
        return array("kind" => "", "summary" => "", "details" => array());
    }
    $details = array();
    $m = MediaDiagnoseMountOf($dir);
    $line = MediaDiagnoseMountLine($dir);
    if ($line != "") {
        $details[] = $line;
    }
    if ($m !== null) {
        $u = MediaDiagnoseUnpartitioned($m[0]);
        if ($u != "") {
            $details[] = $u;
        }
    }
    return array("kind" => $kind, "summary" => $summary, "details" => $details);
}

