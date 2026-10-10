<?php
// Atomic server-side update lock for the web-driven update entry points.
//
// The client-side busy guards (about.php / fpp.js / menu.inc, via
// UpdateActivityBusySide()) are UX-only: two browsers can both pass them
// before either poll observes the other request, and the PHP entry points
// are directly callable. This lock closes that race atomically: every update
// starter takes a non-blocking exclusive flock on one shared file before
// doing any mutating work, and holds it for the whole request. A contender
// gets HTTP 409 + a plain-text message instead of stacking a second update
// on top of the first.
//
// Scope: FPP software / OS / branch / version / rebuild flows driven from
// the web UI (manualUpdate.php, upgradefpp.php, upgradeOS.php,
// gitCheckoutVersion.php, changebranch.php, rebuildfpp.php). The fine-grained
// git-repo lock (scripts/functions runGitLocked, media/tmp/fpp-git-repo.lock)
// is separate and stays as-is; this is the coarse whole-update lock above it.
//
// Safety properties (production constraints):
// - Fail-open: if flock is unavailable or the lock file cannot be opened,
//   the update proceeds exactly as before (returns null). Logging must never
//   break an update, and neither must locking.
// - No stale locks: the kernel releases flock when the holder's FD closes,
//   including kills/crashes. No PID files, no timestamps to go stale.
// - No new worker cost: the holder is the already-running update request
//   (which streams for the whole run anyway); contenders exit immediately.
// - downloadOnly (OS image fetch without apply) DOES take the lock: it
//   writes the same upload/<file> an in-flight apply reads, so two
//   overlapping downloads/applies would corrupt each other.
// - 409 is sent only if headers are still sendable (entry points that
//   already echoed HTML still show the message in the streamed dialog).

/**
 * Resolve the shared update lock file. Lives under the media tmp dir (NOT
 * /tmp) so Apache/PHP and sudo'd scripts share it, mirroring the git-repo
 * lock location. The dir is fpp-owned; root can lock regardless of owner.
 *
 * @return string
 */
function UpdateLockFile()
{
    global $settings, $mediaDirectory;
    $base = '';
    if (isset($settings['mediaDirectory']) && $settings['mediaDirectory'] != '') {
        $base = $settings['mediaDirectory'];
    } elseif (isset($mediaDirectory) && $mediaDirectory != '') {
        $base = $mediaDirectory;
    } else {
        $base = '/home/fpp/media';
    }
    $dir = rtrim($base, '/') . '/tmp';
    @mkdir($dir, 0775, true);
    return $dir . '/fpp-update.lock';
}

/**
 * Take the update lock or refuse with HTTP 409.
 *
 * Must be called after input validation and before ANY mutating work
 * (downloads, START markers, system() calls). No-op when this request
 * already holds the lock (so upgradeOS.php can call it at both the
 * download and apply points safely).
 *
 * @param string $op     Operation label for logs (e.g. 'fpp-update').
 * @param string $target Target label for logs (branch, version, image).
 * @return resource|null Lock handle (held open for the request) or null
 *                       when locking is unavailable (fail-open: proceed).
 */
function UpdateLockAcquireOrConflict($op, $target = '')
{
    // Already holding it in this request (e.g. second call site) — proceed.
    if (isset($GLOBALS['updateLockHandle']) && is_resource($GLOBALS['updateLockHandle'])) {
        return $GLOBALS['updateLockHandle'];
    }
    if (!function_exists('flock')) {
        return null;
    }
    $file = UpdateLockFile();
    // 'e' (close-on-exec): without it the FD is inherited by child processes
    // (git_pull is started without sudo's FD-closing), so a background child
    // keeps the flock after the PHP request exits and later starters get 409
    // until that child dies.
    $fh = @fopen($file, 'c+e');
    if (!$fh) {
        $fh = @fopen($file, 'a+e');
    }
    if (!$fh) {
        return null;
    }
    if (!@flock($fh, LOCK_EX | LOCK_NB)) {
        @fclose($fh);
        if (!headers_sent()) {
            http_response_code(409);
            header('Content-Type: text/plain');
        }
        echo "Another FPP update is already running. Wait for it to finish before starting a new one.\n";
        exit(0);
    }
    $GLOBALS['updateLockHandle'] = $fh;
    register_shutdown_function(function () {
        if (isset($GLOBALS['updateLockHandle']) && is_resource($GLOBALS['updateLockHandle'])) {
            @flock($GLOBALS['updateLockHandle'], LOCK_UN);
            @fclose($GLOBALS['updateLockHandle']);
        }
    });
    return $fh;
}
