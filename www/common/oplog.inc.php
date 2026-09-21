<?php
// Shared operation logging for the long-running, web-driven operations that
// have a matching shell-side helper in scripts/common (startOpLog).
//
// The shell wrappers log what they *ran*; these helpers log what the PHP driver
// *decided*. Both halves append to the same file in the same syslog-style
// format, so one operation reads as one run regardless of which side produced a
// given line:
//
//     <date time> [<op> <target>] <line>
//
// Without the PHP half, decisions that never reach a script leave no trace at
// all -- the plugin package/credentials/depth gates all refuse before anything
// is cloned, and the OS upgrade's download/verify phases are pure PHP, so a
// failure there was invisible in the log and in the Support Zip.
//
// Safe to interleave with the shell side's tee: each call is a single
// line-sized O_APPEND write (LOCK_EX). Being append-based (open/write/close per
// call, no held FD) also means these writes follow logrotate's rename correctly.
//
// Apache runs as fpp, but that is NOT on its own enough to make these writes
// land: the shell half runs via sudo (root), and whichever half creates the log
// first owns it -- root got there first and left it root:root 0644, so every
// PHP line was rejected. The writes below are @-suppressed best-effort, so that
// failed SILENTLY. scripts/common's ensureLogFile() now chowns each log to fpp
// on the shell side (root can write regardless of owner), which is what keeps
// both halves able to append. Do not "simplify" that away.
//
// Best-effort by design -- logging must never break an operation, so failures
// are swallowed.

/**
 * The shared logs directory. $logDirectory comes from config.php, which every
 * caller loads; the literal is only the fallback for a context that somehow has
 * not (an isolated CLI harness). Defined once here rather than at each writer so
 * the two cannot drift apart.
 *
 * @return string
 */
function OpLogDir()
{
	global $logDirectory;

	return isset($logDirectory) && $logDirectory != '' ? $logDirectory : '/home/fpp/media/logs';
}

/**
 * Sanitise a value that gets interpolated into a log line's PREFIX.
 *
 * $msg is split on newlines and every line gets its own prefix, so newlines
 * there are handled. The prefix fields are not: a CR/LF in one of them ends the
 * line early, and whatever follows reads as a genuine, independently-timestamped
 * entry in a log that ships in the Support Zip.
 *
 * This is reachable from user input, not theoretical: upgradeOS.php passes
 * basename($_GET['os']) as $target, and basename() strips path components, not
 * newlines; copystorage.php builds its target from $_GET via escapeshellcmd(),
 * which escapes newlines for the SHELL but leaves the raw byte in the string.
 * Sanitising here, at the sink, covers every caller including future ones --
 * rather than trusting each call site to remember.
 *
 * @param string $v
 * @return string
 */
function OpLogPrefixField($v)
{
	return str_replace(array("\r", "\n"), ' ', $v);
}

/**
 * Appends lines to $logFile in the shared logs directory, one prefixed line per
 * line of $msg.
 *
 * @param string $logFile Bare filename, e.g. 'fpp_system_upgrades.log'.
 * @param string $op      Operation, e.g. 'install', 'os-upgrade'.
 * @param string $target  What the operation acts on (plugin name, version, image).
 * @param string $msg     Message; may be multi-line.
 * @return void
 */
function OpLog($logFile, $op, $target, $msg)
{
	$dir = OpLogDir();
	$stamp = date('Y-m-d H:i:s') . ' [' . OpLogPrefixField($op) . ' ' . OpLogPrefixField($target) . '] ';
	$out = '';
	// dpkg runs on a pty under apt, so its lines arrive CRLF; the \r handling
	// below would otherwise keep only the "" after the last \r and drop them.
	$msg = str_replace("\r\n", "\n", $msg);
	// Echoed messages are wrapped in blank lines for readability in the progress
	// dialog; those carry no meaning in a timestamped log, so skip them rather
	// than emit bare-prefix lines.
	foreach (explode("\n", trim($msg, "\n")) as $line) {
		// Progress bars (wget) are \r-heavy: keep only the final state of the
		// line so a download cannot flood the log with thousands of redraws.
		if (strpos($line, "\r") !== false) {
			$parts = explode("\r", $line);
			$line = end($parts);
		}
		$line = rtrim($line);
		if (trim($line) !== '') {
			$out .= $stamp . $line . "\n";
		}
	}
	if ($out !== '') {
		@file_put_contents($dir . '/' . $logFile, $out, FILE_APPEND | LOCK_EX);
	}
}

/**
 * Appends lines to logs/fppd.log in fppd's own line shape, one prefixed line per
 * line of $msg. The PHP mirror of fppdLogLine() in scripts/common.
 *
 * fppd.log is the timeline: fppd itself, the fppd_start/stop/restart scripts,
 * fppinit, and the operation breadcrumbs all append to it, so it reads as one
 * sequence of what happened to the box. Every line therefore has to name the
 * program that wrote it -- the [facility] field is a subsystem within fppd, not
 * a program.
 *
 * Prefer an existing fppd facility (MediaOut, Plugin, General, ...) so that a
 * PHP line and fppd's own lines for the same subsystem read alike.
 *
 * @param string $program  Program name, e.g. 'run_mp3gain.php'.
 * @param string $facility Facility, e.g. 'MediaOut'.
 * @param string $msg      Message; may be multi-line.
 * @return void
 */
function FppdLogLine($program, $facility, $msg)
{
	$dir = OpLogDir();
	$now = microtime(true);
	$stamp = date('Y-m-d H:i:s', (int)$now) . '.' . sprintf('%03d', (int)(($now - floor($now)) * 1000))
		. ' ' . OpLogPrefixField($program) . '(' . getmypid() . ') [' . OpLogPrefixField($facility) . '] ';
	$out = '';
	foreach (explode("\n", trim($msg, "\n")) as $line) {
		if (strpos($line, "\r") !== false) {
			$parts = explode("\r", $line);
			$line = end($parts);
		}
		$line = rtrim($line);
		if (trim($line) !== '') {
			$out .= $stamp . $line . "\n";
		}
	}
	if ($out !== '') {
		@file_put_contents($dir . '/fppd.log', $out, FILE_APPEND | LOCK_EX);
	}
}

/**
 * Records FPP-software and FPP OS upgrade activity in logs/fpp_system_upgrades.log,
 * the same file scripts/common's startUpgradeLog() appends to.
 */
function UpgradeLog($op, $target, $msg)
{
	OpLog('fpp_system_upgrades.log', $op, $target, $msg);
}

/**
 * Echo a message to the caller (the streaming progress dialog) AND record it in
 * logs/fpp_system_upgrades.log. Echo behaviour is deliberately unchanged.
 */
function UpgradeEchoLog($op, $target, $msg)
{
	echo $msg;
	UpgradeLog($op, $target, $msg);
}
