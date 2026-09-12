<?php
header("Access-Control-Allow-Origin: *");
?>
<!DOCTYPE html>
<html lang="en">
<?
include 'common/htmlMeta.inc';
$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
?>

<head>
	<title>
		Change Branch
	</title>
</head>

<body>
	<h2>Changing Branch</h2>
	<pre id="output">
<?php
echo "==================================================================================\n";

$rawBranch = $_GET['branch'] ?? '';
if (!preg_match('/^[A-Za-z0-9_.\/-]+$/', $rawBranch) || strpos($rawBranch, '..') !== false || $rawBranch === '' || $rawBranch[0] === '-') {
	http_response_code(400);
	echo "Invalid branch";
	exit(0);
}
$branch = $rawBranch;
$rawRemote = $_GET['remote'] ?? 'origin';
if ($rawRemote === 'pull-requests') {
	// synthetic remote for PR mode – validated explicitly, not via regex
} elseif (!preg_match('/^[a-zA-Z0-9_-]+$/', $rawRemote)) {
	$rawRemote = 'origin';
}
$remote = $rawRemote;
$prNumber = 0;
if ($remote === 'pull-requests') {
	if (isset($_GET['pr'])) {
		$prNumber = intval($_GET['pr']);
	} elseif (preg_match('#^pull/(\d+)/head$#', $branch, $m)) {
		$prNumber = intval($m[1]);
	} elseif (preg_match('#^pr-(\d+)$#', $branch, $m)) {
		$prNumber = intval($m[1]);
	}
	if ($prNumber <= 0) {
		http_response_code(400);
		echo "Invalid PR number";
		exit(0);
	}
}

// Pull Requests synthetic remote – fetch pull/<num>/head from origin via lock
$gitDir = escapeshellarg(dirname(dirname(__FILE__)) . "/.git");
$prFetchDone = false;
if ($remote === 'pull-requests') {
	$mediaDirForLock = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : (isset($mediaDirectory) ? $mediaDirectory : "/tmp");
	$gitLock = escapeshellarg($mediaDirForLock . "/tmp/fpp-git-repo.lock");
	$fetchInner = "git --git-dir=$gitDir fetch origin pull/" . $prNumber . "/head:refs/remotes/origin/pull/" . $prNumber . "/head 2>&1";
	if (is_executable("/usr/bin/flock")) {
		exec("$SUDO flock -w 60 -x $gitLock bash -c " . escapeshellarg($fetchInner), $fetchOut, $fetchRet);
	} else {
		exec("$SUDO bash -c " . escapeshellarg($fetchInner), $fetchOut, $fetchRet);
	}
	foreach ($fetchOut as $line) {
		echo htmlspecialchars($line) . "\n";
	}
	flush(); ob_flush();
	$prFetchDone = true;
	if ($fetchRet != 0) {
		echo "Failed to fetch PR #$prNumber from origin\n";
		// Let git_branch report error too, don't exit yet
	}
	// Local branch will be pr-<num>, tracking origin/pull/<num>/head
	$branch = "pr-" . $prNumber;
	$remote = "origin";
}

// If remote is a GitHub fork (matches saved gitHubUser), ensure the git remote exists
$isForkRemote = false;
$forkUser = isset($settings['gitHubUser']) ? trim($settings['gitHubUser']) : '';
$forkPat = isset($settings['gitHubPAT']) ? trim($settings['gitHubPAT']) : '';
if (!$prFetchDone && $remote !== 'origin' && $remote !== 'newfeatures' && $remote !== 'pull-requests') {
	if ($forkUser !== '' && strtolower($remote) === strtolower($forkUser)) {
		// normalize remote to the canonical saved username casing for git remote operations
		$remote = $forkUser;
		$isForkRemote = true;
		// Check if remote already exists
		exec("$SUDO git --git-dir=$gitDir remote get-url " . escapeshellarg($remote) . " 2>&1", $chkOut, $chkRet);
		if ($chkRet != 0) {
			if ($forkPat !== '') {
				$remoteUrl = "https://" . rawurlencode($forkUser) . ":" . rawurlencode($forkPat) . "@github.com/" . rawurlencode($forkUser) . "/fpp.git";
			} else {
				$remoteUrl = "https://github.com/" . rawurlencode($forkUser) . "/fpp.git";
			}
			exec("$SUDO git --git-dir=$gitDir remote add " . escapeshellarg($remote) . " " . escapeshellarg($remoteUrl) . " 2>&1", $addOut, $addRet);
		} else {
			// Update URL to include PAT if present (keeps private fork fetches authenticated)
			if ($forkPat !== '') {
				$remoteUrl = "https://" . rawurlencode($forkUser) . ":" . rawurlencode($forkPat) . "@github.com/" . rawurlencode($forkUser) . "/fpp.git";
				exec("$SUDO git --git-dir=$gitDir remote set-url " . escapeshellarg($remote) . " " . escapeshellarg($remoteUrl) . " 2>&1", $updOut, $updRet);
			}
		}
	}
}

// Fetch the fork remote so that git_branch's show-ref validation succeeds.
// Newly-added remotes have no refs yet; without this the script aborts with
// "Invalid Branch Name" even though the branch exists on GitHub.
if ($isForkRemote) {
	$fetchOut = array();
	$fetchRet = 0;
	// Use same lock as git_branch / git_fetch to avoid FETCH_HEAD races
	$mediaDirForLock = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : (isset($mediaDirectory) ? $mediaDirectory : "/tmp");
	$gitLock = escapeshellarg($mediaDirForLock . "/tmp/fpp-git-repo.lock");
	$fetchInner = "git --git-dir=$gitDir fetch " . escapeshellarg($remote) . " 2>&1";
	if (is_executable("/usr/bin/flock")) {
		exec("$SUDO flock -w 60 -x $gitLock bash -c " . escapeshellarg($fetchInner), $fetchOut, $fetchRet);
	} else {
		exec("$SUDO bash -c " . escapeshellarg($fetchInner), $fetchOut, $fetchRet);
	}
	foreach ($fetchOut as $line) {
		echo htmlspecialchars($line) . "\n";
	}
	flush(); ob_flush();
	// Let git_branch report a proper error if fetch failed; don't abort here.
}

$command = $SUDO . " " . escapeshellarg($fppDir . "/scripts/git_branch") . " " . escapeshellarg($branch) . " " . escapeshellarg($remote) . " 2>&1";

echo "Command: $command\n";
echo "----------------------------------------------------------------------------------\n";
flush();
ob_flush();

// Stream output and auto-scroll
// Only output script tags if being viewed directly in a browser (not via curl/proxy)
$isDirectView = !empty($_SERVER['HTTP_USER_AGENT']) && strpos($_SERVER['HTTP_USER_AGENT'], 'curl') === false;

$handle = popen($command, 'r');
while (!feof($handle)) {
	$buffer = fgets($handle);
	echo $buffer;
	flush();
	ob_flush();
	if ($isDirectView) {
		echo '<script>window.scrollTo(0, document.body.scrollHeight);</script>';
		flush();
		ob_flush();
	}
}
pclose($handle);
echo "\n";
?>

==========================================================================
</pre>
	<?php if ($isDirectView): ?>
		<a href='index.php'>Go to FPP Main Status Page</a><br>
		<script>
			// Ensure we scroll to bottom on page load
			window.scrollTo(0, document.body.scrollHeight);
		</script>
	<?php endif; ?>
</body>

</html>