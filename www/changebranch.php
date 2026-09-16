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
	$fetchInner = "git --git-dir=$gitDir fetch origin +pull/" . $prNumber . "/head:refs/remotes/origin/pull/" . $prNumber . "/head 2>&1";
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

// ---------------------------------------------------------------------------
// Config-migration gate for PR mode.
//
// scripts/git_branch runs upgrade_config from whatever tree it just checked
// out, and upgrade_config only ever moves /etc/fpp/config_version FORWARD --
// nothing in the tree writes it back down. The migrations themselves act
// outside git (apt installs, units in /lib/systemd/system, the Apache site
// config), so switching back to a release branch undoes none of it.
//
// That bites hardest when hopping between PRs, because the numbering collides:
// two PRs branched off config 143 both add upgrade/144. Run A's and the device
// is at 144, so B's 144 is skipped -- and stays skipped once B merges and its
// 144 becomes the real one. Silent, permanent config skew on a box that looks
// healthy.
//
// So say so, once, before committing to it. Only the ref fetch has happened at
// this point; fppd is still up and nothing has been cleaned, so bailing out
// here leaves the device exactly as it was.
// ---------------------------------------------------------------------------

/**
 * Config migrations present on $ref that this device has not run yet.
 *
 * @param string $gitDir Shell-quoted path to the FPP .git directory.
 * @param string $ref    Ref to inspect (e.g. refs/remotes/origin/pull/42/head).
 * @param string $SUDO   Privilege-escalation prefix from common.php.
 * @return array Map of version number => description, ascending. Empty when the
 *               device's config version cannot be read, since without a
 *               baseline every migration would look pending.
 */
function PendingConfigMigrations($gitDir, $ref, $SUDO)
{
	$current = 0;
	if (is_readable("/etc/fpp/config_version")) {
		$current = intval(trim(file_get_contents("/etc/fpp/config_version")));
	}
	if ($current <= 0) {
		return array();
	}

	$dirs = array();
	$ret = 0;
	exec("$SUDO git --git-dir=$gitDir ls-tree --name-only " . escapeshellarg($ref) . " upgrade/ 2>/dev/null", $dirs, $ret);
	if ($ret != 0) {
		return array();
	}

	$pending = array();
	foreach ($dirs as $line) {
		if (!preg_match('#^upgrade/([0-9]+)$#', trim($line), $m)) {
			continue;
		}
		$version = intval($m[1]);
		if ($version <= $current) {
			continue;
		}

		// Migrations open with "# Upgrade <n>: <what it does>". Older ones
		// predate the convention, so an empty description is expected, not an
		// error -- the version number alone is the part that matters here.
		$description = '';
		$script = array();
		$sRet = 0;
		exec("$SUDO git --git-dir=$gitDir show " . escapeshellarg($ref . ":upgrade/" . $version . "/upgrade.sh") . " 2>/dev/null | head -40", $script, $sRet);
		if ($sRet == 0) {
			foreach ($script as $sLine) {
				if (preg_match('/^#\s*Upgrade\s+[0-9]+:\s*(.+?)\s*$/', $sLine, $sm)) {
					$description = $sm[1];
					break;
				}
			}
		}

		$pending[$version] = $description;
	}

	ksort($pending);
	return $pending;
}

if ($prFetchDone && ($_GET['ackcfg'] ?? '') !== '1') {
	$pending = PendingConfigMigrations($gitDir, "refs/remotes/origin/pull/" . $prNumber . "/head", $SUDO);
	if (!empty($pending)) {
		$currentCfg = trim(@file_get_contents("/etc/fpp/config_version"));
		$continueUrl = "changebranch.php?branch=pr-" . $prNumber . "&remote=pull-requests&pr=" . $prNumber . "&ackcfg=1";

		echo "\n";
		echo "!! PR #" . $prNumber . " ADVANCES THIS DEVICE'S CONFIG VERSION -- THIS IS NOT REVERSIBLE !!\n";
		echo "\n";
		echo "This device is at config version " . htmlspecialchars($currentCfg) . ". Switching to this PR will run:\n\n";
		foreach ($pending as $version => $description) {
			echo "    upgrade/" . $version;
			if ($description !== '') {
				echo " - " . htmlspecialchars($description);
			}
			echo "\n";
		}
		echo "\n";
		echo "These migrations change the system outside the git tree (packages, systemd\n";
		echo "units, Apache and FPP config files). Switching back to a release branch\n";
		echo "afterwards does NOT undo them, and does NOT lower the config version.\n";
		echo "\n";
		echo "Because PRs branched from the same point reuse the same migration number,\n";
		echo "running this one means a different PR's migration -- and the released one,\n";
		echo "once it merges -- will be skipped on this device from now on.\n";
		echo "\n";
		echo "Only continue on a device you are willing to reflash.\n";
		echo "\n";
		echo "==================================================================================\n";
		?>
</pre>
		<a href='<?= htmlspecialchars($continueUrl) ?>'>Continue and switch to PR #<?= intval($prNumber) ?></a><br>
		<a href='settings.php#settings-developer'>Cancel and go back to Developer Settings</a>
	</body>

</html>
<?php
		exit(0);
	}
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