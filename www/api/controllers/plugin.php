<?

// Shared system-package helpers (hardened apt + per-requester ownership) used
// when installing/removing a plugin's declared package dependencies.
require_once __DIR__ . '/../../common/packages.inc.php';

// Shared operation logging (OpLog), the PHP half of scripts/common's startOpLog.
require_once __DIR__ . '/../../common/oplog.inc.php';

/**
 * Appends lines to the shared logs/fpp_plugin_manager.log in the same
 * syslog-style format scripts/common's startPluginLog() writes:
 * "<date time> [<op> <plugin>] <line>".
 *
 * The wrapper scripts log what they *ran*; this logs what the Plugin Manager
 * *decided*. Without it the decisions that never reach a script leave no trace
 * at all: the package/credentials/depth gates all refuse before anything is
 * cloned, so a failed install was invisible in the log and in the Support Zip.
 * Worse, a dependency failure aborts *after* a successful clone and deletes the
 * partial install (CleanupPartialPluginInstall) -- leaving a clean
 * "install FINISH (rc=0)" block in the log for a plugin no longer on disk.
 */
function PluginLog($op, $plugin, $msg)
{
	OpLog('fpp_plugin_manager.log', $op, $plugin, $msg);
}

// True when the caller asked for streamed progress output (?stream=...).
function PluginStreaming($stream)
{
	return isset($stream) && $stream != "false";
}

/**
 * Echo a message to the caller's streaming progress dialog AND record it in
 * fpp_plugin_manager.log. The echo happens ONLY when streaming: a non-streaming
 * caller gets a JSON body, and echoing into it produced output like
 * "Installed plugin 'X'.\n{"Status":"OK"}" -- unparseable as strict JSON.
 * The log write is unconditional; it is what makes a silent install failure
 * diagnosable, so it must never depend on how the caller asked for output.
 */
function PluginEchoLog($op, $plugin, $msg, $stream)
{
	if (PluginStreaming($stream)) {
		echo $msg;
	}
	PluginLog($op, $plugin, $msg);
}

// True if a dependencies block declares at least one non-empty apt package.
function DepsRequirePackages($deps)
{
	if (!is_array($deps) || !isset($deps['packages']) || !is_array($deps['packages'])) {
		return false;
	}
	foreach ($deps['packages'] as $p) {
		if (is_string($p) && $p !== '') {
			return true;
		}
	}
	return false;
}

// True if a dependencies block declares at least one non-empty Python (pip) package.
function DepsRequirePython($deps)
{
	if (!is_array($deps) || !isset($deps['python']) || !is_array($deps['python'])) {
		return false;
	}
	foreach ($deps['python'] as $p) {
		if (is_string($p) && $p !== '') {
			return true;
		}
	}
	return false;
}

// True if 'pip' (via `python3 -m pip`) is available. FPP's own OS image
// provisions python3-pip via apt (see SD/FPP_Install.sh) specifically so
// plugins have a sanctioned way to install Python packages -- see
// PLUGIN_GUIDELINES.md #6.2 in fpp-plugin-Template. Cached per-request; this
// is checked at most twice (posted info, then the authoritative re-check
// after clone).
function PipAvailable()
{
	static $available = null;
	if ($available === null) {
		$out = array();
		$rc = 1;
		exec('python3 -m pip --version 2>/dev/null', $out, $rc);
		$available = ($rc === 0 && count($out) > 0);
	}
	return $available;
}

// Removes a partially-installed plugin directory (and any linkName symlink) so a
// refused/failed install does not leave a half-installed plugin behind.
function CleanupPartialPluginInstall($plugin, $linkName = null)
{
	global $settings, $SUDO;
	if (is_string($linkName) && $linkName !== '') {
		exec($SUDO . " rm -f " . escapeshellarg($settings['pluginDirectory'] . '/' . $linkName));
	}
	exec($SUDO . " rm -rf " . escapeshellarg($settings['pluginDirectory'] . '/' . $plugin));
}

/**
 * Get all plugins
 *
 * Get list of installed plugins.
 *
 * @route GET /api/plugin
 * @response 200 List of installed plugin names
 * ```json
 * ["fpp-brightness", "fpp-matrixtools", "fpp-vastfmt"]
 * ```
 */
function GetInstalledPlugins()
{
	global $settings;
	$plugins = array();

	$dir = $settings['pluginDirectory'];

	if ($dh = opendir($dir)) {
		while (($file = readdir($dh)) !== false) {
			if (
				(!in_array($file, array('.', '..'))) &&
				(is_dir($dir . '/' . $file)) &&
				(file_exists($dir . '/' . $file . '/pluginInfo.json'))
			) {
				array_push($plugins, $file);
			}
		}
	}

	return json($plugins);
}

/**
 * Install plugin
 *
 * Install a new plugin. The request body is a `pluginInfo.json` structure
 * with `branch` and `sha` fields added to specify which branch and commit
 * to install.
 *
 * @route POST /api/plugin
 * @body {"repoName": "fpp-matrixtools", "name": "MatrixTools", "author": "Chris Pinkham (CaptainMurdoch)", "srcURL": "https://github.com/cpinkham/fpp-matrixtools.git", "branch": "master", "sha": ""}
 * @response 200 Plugin installed
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function InstallPlugin()
{
	global $settings, $_REQUEST;
	$result = array();

	$pluginInfoJSON = "";
	$postdata = fopen("php://input", "r");
	while ($data = fread($postdata, 1024 * 16)) {
		$pluginInfoJSON .= $data;
	}
	fclose($postdata);

	$pluginInfo = json_decode($pluginInfoJSON, true);
	if (!is_array($pluginInfo) || !isset($pluginInfo['repoName'])) {
		$result['Status'] = 'Error';
		$result['Message'] = 'Invalid pluginInfo (missing repoName)';
		return json($result);
	}

	$stream = isset($_REQUEST['stream']) ? $_REQUEST['stream'] : null;
	$streaming = PluginStreaming($stream);
	$plugin = escapeshellcmd($pluginInfo['repoName']);

	if (file_exists($settings['pluginDirectory'] . '/' . $plugin)) {
		if ($streaming) {
			DisableOutputBuffering();
			echo "The (" . $plugin . ") plugin is already installed\n";
			return "\nDone\n";
		}
		$result['Status'] = 'Error';
		$result['Message'] = 'The (' . $plugin . ') plugin is already installed';
		return json($result);
	}

	if ($streaming) {
		DisableOutputBuffering();
	}

	// $visited guards against dependency cycles (A depends on B depends on A)
	// across the recursive install below.
	$visited = array();
	$ok = InstallPluginFromInfo($pluginInfo, $visited, $stream, 0);

	if ($streaming) {
		return "\nDone\n";
	}
	$result['Status'] = $ok ? 'OK' : 'Error';
	$result['Message'] = $ok ? '' : 'Could not properly install plugin';
	return json($result);
}

/**
 * Installs a single plugin from a decoded pluginInfo structure, resolving its
 * declared dependencies (packages, scripts, plugins) BEFORE running the
 * plugin's own fpp_install.sh. Recurses into dependency plugins with a shared
 * $visited set (cycle guard) and a depth cap. Output is streamed to the client
 * when $stream is truthy. Returns true on success.
 */
function InstallPluginFromInfo($pluginInfo, &$visited, $stream, $depth = 0)
{
	global $settings, $fppDir, $SUDO;

	$streaming = PluginStreaming($stream);

	if (!is_array($pluginInfo) || !isset($pluginInfo['repoName'])) {
		// No repoName to tag the log line with -- that IS the error.
		PluginEchoLog('install', 'unknown', "\nERROR: dependency plugin info missing repoName\n", $stream);
		return false;
	}
	$repoName = $pluginInfo['repoName'];
	$plugin = escapeshellcmd($repoName);

	// Cycle guard.
	if (isset($visited[$repoName])) {
		return true;
	}
	$visited[$repoName] = true;

	if ($depth > 8) {
		PluginEchoLog('install', $repoName, "\nERROR: plugin dependency chain too deep at '$repoName'; aborting.\n", $stream);
		return false;
	}

	// Already installed -> dependency is satisfied.
	if (file_exists($settings['pluginDirectory'] . '/' . $plugin)) {
		if ($depth > 0 && $streaming) {
			echo "\nDependency plugin '$plugin' is already installed.\n";
		}
		return true;
	}

	// Dependencies can be declared top-level (applies to every version) and/or
	// on the specific versions[] entry that matches this system (additional to
	// the top-level ones -- e.g. a package whose name differs between the FPP9
	// and FPP10 image). Merge them once, up front.
	$declaredDeps0 = MergePluginDependencies(
		isset($pluginInfo['dependencies']) ? $pluginInfo['dependencies'] : null,
		SelectPluginVersionEntry($pluginInfo)
	);

	// Refuse up front if the plugin declares required apt packages but this
	// platform has no apt -- installing it would leave it broken. A plugin that
	// genuinely needs packages should restrict itself with platforms[]; if the
	// author forgot, we catch it here rather than half-installing. (Re-checked
	// after clone against the repo's own pluginInfo.json, which is authoritative.)
	if (DepsRequirePackages($declaredDeps0) && !AptAvailable()) {
		$pkgs = implode(', ', $declaredDeps0['packages']);
		$plat = isset($settings['Platform']) ? $settings['Platform'] : 'this platform';
		PluginEchoLog('install', $repoName, "\nERROR: '$repoName' requires system packages ($pkgs) but $plat does not support system packages. Refusing to install.\n", $stream);
		return false;
	}

	// Same idea for Python package dependencies: refuse up front rather than
	// half-install if 'pip' isn't available. FPP's own OS image provisions
	// python3-pip via apt (SD/FPP_Install.sh) specifically for this, so its
	// absence means a non-standard/unprovisioned system, not a normal
	// per-platform gap.
	if (DepsRequirePython($declaredDeps0) && !PipAvailable()) {
		PluginEchoLog('install', $repoName, "\nERROR: '$repoName' requires Python package dependencies but 'pip' is not available on this system. Refusing to install.\n", $stream);
		return false;
	}

	$srcURL = isset($pluginInfo['srcURL']) ? $pluginInfo['srcURL'] : '';
	$branch = escapeshellcmd(isset($pluginInfo['branch']) && $pluginInfo['branch'] !== '' ? $pluginInfo['branch'] : 'master');
	$sha = isset($pluginInfo['sha']) ? $pluginInfo['sha'] : '';
	$infoURL = isset($pluginInfo['infoURL']) ? $pluginInfo['infoURL'] : '';
	$useCredentials = isset($pluginInfo['useCredentials']) && $pluginInfo['useCredentials'];

	// Inject GitHub credentials for github URLs when configured. Only modifies
	// github.com / raw.githubusercontent.com URLs so creds never leak elsewhere.
	$injectedURL = InjectGitHubCredentials($srcURL);
	if ($injectedURL !== false) {
		$srcURL = $injectedURL;
	} else if ($useCredentials) {
		PluginEchoLog('install', $repoName, "\nERROR: Use Credentials was selected but GitHub user name and/or Personal Access Token are not configured on the Developer settings page.\n", $stream);
		return false;
	}

	// Clone ONLY -- the plugin's own fpp_install.sh is deferred (via
	// FPP_SKIP_INSTALL_SCRIPT) so dependencies can be resolved first.
	$return_val = 0;
	$envPrefix = "export FPP_SKIP_INSTALL_SCRIPT=1; export SUDO=\"" . $SUDO . "\"; export PLUGINDIR=\"" . $settings['pluginDirectory'] . "\"; ";
	$cloneCmd = $envPrefix . "$fppDir/scripts/install_plugin $plugin \"$srcURL\" \"$branch\" \"$sha\"";
	if ($streaming) {
		system($cloneCmd, $return_val);
	} else {
		exec($cloneCmd, $o, $return_val);
		unset($o);
	}
	if ($return_val != 0) {
		PluginEchoLog('install', $repoName, "\nERROR: failed to clone plugin '$plugin'.\n", $stream);
		return false;
	}

	// Determine the authoritative pluginInfo: the repo's own pluginInfo.json if
	// it ships one, otherwise the one fetched from infoURL. Defer writing the
	// fetched copy / creating the linkName symlink until after the package gate
	// so a refusal leaves nothing behind.
	$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';
	$fetchedInfo = null;
	$data = null;
	if (file_exists($infoFile)) {
		$data = json_decode(file_get_contents($infoFile), true);
	} else if ($infoURL !== '') {
		$fetchedInfo = FetchURLWithGitHubCredentials($infoURL);
		$data = json_decode($fetchedInfo, true);
	}
	if (!is_array($data)) {
		$data = $pluginInfo;
	}
	// Authoritative dependency set: the cloned repo's own pluginInfo.json may
	// declare deps the posted info did not, and this also re-selects the
	// versions[] entry against the AUTHORITATIVE data (not the posted body),
	// merging in that entry's own 'dependencies' block per MergePluginDependencies().
	$deps = MergePluginDependencies(
		isset($data['dependencies']) ? $data['dependencies'] : null,
		SelectPluginVersionEntry($data)
	);

	// Authoritative package gate: the cloned repo's own pluginInfo.json may
	// declare packages the posted info did not. Same rule -- refuse on a
	// platform without apt, and remove the partial clone.
	if (DepsRequirePackages($deps) && !AptAvailable()) {
		$pkgs = implode(', ', $deps['packages']);
		$plat = isset($settings['Platform']) ? $settings['Platform'] : 'this platform';
		// Logged, not just echoed: the clone above already wrote its own
		// "install FINISH (rc=0)" block, so without this the log would show a
		// clean install for a plugin this line is about to delete.
		PluginEchoLog('install', $repoName, "\nERROR: '$repoName' requires system packages ($pkgs) but $plat does not support system packages. Refusing to install.\nRemoving the partial install of '$plugin'.\n", $stream);
		CleanupPartialPluginInstall($plugin);
		return false;
	}

	// Authoritative Python-dependency gate, same reasoning as the package gate above.
	if (DepsRequirePython($deps) && !PipAvailable()) {
		PluginEchoLog('install', $repoName, "\nERROR: '$repoName' requires Python package dependencies but 'pip' is not available on this system. Refusing to install.\nRemoving the partial install of '$plugin'.\n", $stream);
		CleanupPartialPluginInstall($plugin);
		return false;
	}

	// Install is going ahead: commit the fetched pluginInfo.json + linkName.
	$linkName = null;
	if ($fetchedInfo !== null) {
		file_put_contents($infoFile, $fetchedInfo);
		if (isset($data['linkName'])) {
			$linkName = $data['linkName'];
			exec("cd " . $settings['pluginDirectory'] . " && ln -s " . $plugin . " " . escapeshellarg($linkName), $o, $rv);
			unset($o);
		}
	}

	// Resolve declared dependencies BEFORE the plugin's own install script. If a
	// required dependency cannot be installed, refuse and clean up rather than
	// run the plugin's install script against missing prerequisites.
	if ($deps !== null) {
		if (!ResolvePluginDependencies($deps, $repoName, $visited, $stream, $depth)) {
			// Same trap as the package gate above: the clone's own rc=0 block is
			// already in the log, and the cleanup below removes the plugin. Say so.
			PluginEchoLog('install', $repoName, "\nERROR: refusing to complete install of '$plugin' -- a required dependency could not be installed.\nRemoving the partial install of '$plugin'.\n", $stream);
			CleanupPartialPluginInstall($plugin, $linkName);
			return false;
		}
	}

	// Finally, run the plugin's own install script. It was deferred above (via
	// FPP_SKIP_INSTALL_SCRIPT) so dependencies are in place first; hand it back to
	// install_plugin now that they are. That wrapper owns the mode normalization,
	// the scripts/ -> repo-root resolution and the FPPDIR/SRCDIR invocation, so
	// this phase runs identically to a non-deferred install -- and, because it goes
	// through startPluginLog, its output lands in logs/fpp_plugin_manager.log instead of only
	// streaming to the browser dialog (a plugin script can build/fetch for minutes).
	$runCmd = $envPrefix . escapeshellarg($fppDir . '/scripts/install_plugin')
		. ' --run-install-script ' . escapeshellarg($plugin);
	if ($streaming) {
		system($runCmd, $return_val);
	} else {
		exec($runCmd, $o, $return_val);
		unset($o);
	}

	// The only statement that the operation as a whole succeeded -- the wrapper
	// scripts only ever report on their own phase.
	PluginEchoLog('install', $repoName, "\nInstalled plugin '$plugin'.\n", $stream);
	return true;
}

/**
 * Resolves a plugin's dependencies block: system packages (apt, ref-counted to
 * the owning plugin), Python packages (pip, system-wide -- not isolated per
 * plugin), script-repository scripts ("Category/file"), and other plugins
 * (installed transitively). Packages are installed first, then Python
 * packages, then scripts, then dependency plugins. Returns false if a
 * *required* dependency (a declared package, Python package, or a dependency
 * plugin) could not be installed, so the caller can refuse the whole install;
 * script-repository entries are treated as soft.
 */
function ResolvePluginDependencies($deps, $ownerRepo, &$visited, $stream, $depth)
{
	global $settings, $fppDir, $SUDO;
	$streaming = PluginStreaming($stream);
	$ok = true;

	// --- packages (apt) ---
	if (isset($deps['packages']) && is_array($deps['packages']) && count($deps['packages'])) {
		if ($streaming) {
			echo "\n=== Installing package dependencies for $ownerRepo ===\n";
			flush();
		}
		// Refresh package lists once for the whole batch.
		AptGetUpdate();
		foreach ($deps['packages'] as $pkg) {
			if (is_string($pkg) && $pkg !== '') {
				if (!InstallSystemPackage($pkg, $ownerRepo, false)) {
					// Name the package that failed. InstallSystemPackage echoes the
					// apt transcript (and its own error) straight to the caller, so
					// without this the log would record only the downstream
					// "a required dependency could not be installed" refusal --
					// naming the symptom but never the cause. Logged rather than
					// echoed: the caller already saw the apt output above.
					PluginLog('install', $ownerRepo, "ERROR: failed to install required package '$pkg' (apt output above in the install dialog)");
					$ok = false;
				}
			}
		}
	}

	// --- python packages (pip, system-wide) ---
	if (isset($deps['python']) && is_array($deps['python']) && count($deps['python'])) {
		$pyPkgs = array();
		foreach ($deps['python'] as $pkg) {
			if (is_string($pkg) && $pkg !== '') {
				$pyPkgs[] = $pkg;
			}
		}
		if (count($pyPkgs)) {
			if ($streaming) {
				echo "\n=== Installing Python package dependencies for $ownerRepo (pip) ===\n";
				flush();
			}
			$args = implode(' ', array_map('escapeshellarg', $pyPkgs));
			// `pip install --break-system-packages` is required on any current
			// PEP-668-managed image (Debian/RPi OS Bookworm+) -- without it pip refuses
			// outright ("externally managed environment"). This is safe here, not a
			// corruption risk: it installs into /usr/local/lib/python3.x/dist-packages,
			// which is NOT tracked by dpkg (apt-installed python3-* packages live in
			// /usr/lib/python3/dist-packages instead) -- confirmed by inspecting both
			// paths directly. No per-plugin venv/pyproject.toml to create or chown --
			// plugin scripts just call the system "python3" (see PLUGIN_GUIDELINES.md
			// #6.1). Packages are shared system-wide, not isolated per plugin: a version
			// conflict between two plugins' declared deps will surface as a real install
			// failure here, not silently coexist. Note: a plugin needing a Python version
			// other than FPP's system default (e.g. a dependency with no wheel for it) has
			// no built-in FPP mechanism for that -- must be handled by the plugin itself.
			$cmd = $SUDO . " python3 -m pip install --break-system-packages " . $args;
			$rc = 0;
			if ($streaming) {
				system($cmd, $rc);
			} else {
				exec($cmd, $o, $rc);
				unset($o);
			}
			if ($rc !== 0) {
				PluginLog('install', $ownerRepo, "ERROR: 'pip install --break-system-packages' failed for one or more Python package dependencies (exit $rc)");
				$ok = false;
			} else {
				PluginLog('install', $ownerRepo, "Installed Python package dependencies: " . implode(', ', $pyPkgs));
			}
		}
	}

	// --- scripts (script repository "Category/file") ---
	if (isset($deps['scripts']) && is_array($deps['scripts']) && count($deps['scripts'])) {
		if ($streaming) {
			echo "\n=== Installing script dependencies for $ownerRepo ===\n";
			flush();
		}
		foreach ($deps['scripts'] as $entry) {
			if (!is_string($entry) || strpos($entry, '/') === false) {
				// A declared dependency silently not satisfied -- same class as the
				// unresolvable-dependency-plugin case below, so log it too.
				PluginEchoLog('install', $ownerRepo, "\nSkipping malformed script dependency '$entry' (expected 'Category/file').\n", $stream);
				continue;
			}
			list($category, $file) = explode('/', $entry, 2);
			if (!preg_match('#^[A-Za-z0-9._ -]+$#', $category) || !preg_match('#^[A-Za-z0-9._ /-]+$#', $file)) {
				PluginEchoLog('install', $ownerRepo, "\nSkipping script dependency with unsafe characters: '$entry'.\n", $stream);
				continue;
			}
			if ($streaming) {
				echo "\nInstalling script '$entry'...\n";
				flush();
			}
			$cmd = $SUDO . " $fppDir/scripts/installScript " . escapeshellarg($category) . " " . escapeshellarg($file);
			if ($streaming) {
				system($cmd);
			} else {
				exec($cmd, $o);
				unset($o);
			}
		}
	}

	// --- dependency plugins (transitive) ---
	if (isset($deps['plugins']) && is_array($deps['plugins']) && count($deps['plugins'])) {
		if ($streaming) {
			echo "\n=== Installing plugin dependencies for $ownerRepo ===\n";
			flush();
		}
		foreach ($deps['plugins'] as $depName) {
			if (!is_string($depName) || $depName === '') {
				continue;
			}
			if (isset($visited[$depName])) {
				continue;
			}
			if (file_exists($settings['pluginDirectory'] . '/' . escapeshellcmd($depName))) {
				if ($streaming) {
					echo "\nDependency plugin '$depName' is already installed.\n";
				}
				$visited[$depName] = true;
				continue;
			}
			$depInfo = ResolvePluginInfoByName($depName);
			if ($depInfo === null) {
				PluginEchoLog('install', $ownerRepo, "\nERROR: could not resolve dependency plugin '$depName' from pluginList.json (skipping).\n", $stream);
				continue;
			}
			$ver = SelectPluginVersion($depInfo);
			if ($ver !== null) {
				$depInfo['branch'] = $ver['branch'];
				$depInfo['sha'] = $ver['sha'];
			}
			if (!InstallPluginFromInfo($depInfo, $visited, $stream, $depth + 1)) {
				PluginEchoLog('install', $ownerRepo, "\nERROR: dependency plugin '$depName' could not be installed.\n", $stream);
				$ok = false;
			}
		}
	}

	return $ok;
}

/**
 * Resolves a plugin repoName to its pluginInfo structure by consulting the
 * official pluginList.json in FalconChristmas/fpp-data. Only names present in
 * that list can be installed as dependencies (arbitrary URLs are rejected).
 * Returns the decoded pluginInfo (with infoURL set) or null.
 */
function ResolvePluginInfoByName($repoName)
{
	static $pluginList = null;
	if ($pluginList === null) {
		$listJSON = FetchURLWithGitHubCredentials('https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json');
		$decoded = json_decode($listJSON, true);
		$pluginList = (is_array($decoded) && isset($decoded['pluginList']) && is_array($decoded['pluginList'])) ? $decoded['pluginList'] : array();
	}

	$infoURL = null;
	foreach ($pluginList as $entry) {
		if (is_array($entry) && count($entry) >= 2 && $entry[0] === $repoName) {
			$infoURL = $entry[1];
			break;
		}
	}
	if ($infoURL === null) {
		return null;
	}

	$infoJSON = FetchURLWithGitHubCredentials($infoURL);
	$info = json_decode($infoJSON, true);
	if (!is_array($info)) {
		return null;
	}
	$info['infoURL'] = $infoURL;
	return $info;
}

/**
 * Picks the branch/sha to install for a dependency plugin based on the running
 * FPP version and platform. PHP port of the version-window selection in
 * www/plugins.php (LoadPlugin). Falls back to the first version entry when none
 * match. Returns array('branch'=>..., 'sha'=>...) or null.
 */
function SelectPluginVersion($pluginInfo)
{
	$v = SelectPluginVersionEntry($pluginInfo);
	if ($v === null) {
		return null;
	}
	return array(
		'branch' => isset($v['branch']) && $v['branch'] !== '' ? $v['branch'] : 'master',
		'sha' => isset($v['sha']) ? $v['sha'] : ''
	);
}

/**
 * Same matching logic as SelectPluginVersion(), but returns the full matched
 * versions[] entry instead of just branch/sha, so callers can also read a
 * per-version 'dependencies' block (see MergePluginDependencies()). Returns
 * null if $pluginInfo has no versions[] array.
 */
function SelectPluginVersionEntry($pluginInfo)
{
	global $settings;
	if (!isset($pluginInfo['versions']) || !is_array($pluginInfo['versions']) || count($pluginInfo['versions']) === 0) {
		return null;
	}
	$triplet = getFPPVersionTriplet();
	$versions = $pluginInfo['versions'];
	$compatible = -1;
	foreach ($versions as $i => $v) {
		$min = isset($v['minFPPVersion']) ? $v['minFPPVersion'] : '0';
		$max = isset($v['maxFPPVersion']) ? $v['maxFPPVersion'] : '';
		$openMax = ($max === '0' || $max === '0.0' || $max === '');
		$platformsOk = true;
		if (isset($v['platforms']) && is_array($v['platforms'])) {
			$platformsOk = isset($settings['Platform']) && in_array($settings['Platform'], $v['platforms']);
		}
		$minOk = (ComparePluginFPPVersions($min, $triplet) <= 0);
		$maxOk = $openMax || (ComparePluginFPPVersions($max, $triplet) > 0);
		if ($minOk && $maxOk && $platformsOk) {
			$compatible = $i; // last matching entry wins, matching the JS logic
		}
	}
	if ($compatible < 0) {
		$compatible = 0; // fall back to first entry so the dependency still installs
	}
	return $versions[$compatible];
}

/**
 * Merges a plugin's top-level dependencies block with the ones declared on
 * its currently-selected versions[] entry, if any. Per-version dependencies
 * are ADDITIONAL to the top-level ones (e.g. a package whose name differs
 * between the FPP9 and FPP10 image) -- they don't replace them. Arrays are
 * concatenated per-key; a name declared both places is a harmless duplicate
 * (every install path here -- apt/pip/script-repo/plugin -- is a no-op on a
 * repeat). Returns null if neither side declares anything.
 */
function MergePluginDependencies($topLevelDeps, $versionEntry)
{
	$top = is_array($topLevelDeps) ? $topLevelDeps : array();
	$ver = (is_array($versionEntry) && isset($versionEntry['dependencies']) && is_array($versionEntry['dependencies']))
		? $versionEntry['dependencies'] : array();
	if (!count($top) && !count($ver)) {
		return null;
	}
	$merged = array();
	foreach (array('packages', 'python', 'scripts', 'plugins') as $key) {
		$list = array();
		if (isset($top[$key]) && is_array($top[$key])) {
			$list = array_merge($list, $top[$key]);
		}
		if (isset($ver[$key]) && is_array($ver[$key])) {
			$list = array_merge($list, $ver[$key]);
		}
		if (count($list)) {
			$merged[$key] = $list;
		}
	}
	return count($merged) ? $merged : null;
}

// PHP port of versionToNumber() in www/js/fpp.js -- turns a version string into
// a comparable integer.
function PluginVersionToNumber($version)
{
	$version = (string) $version;
	if (strlen($version) > 0 && $version[0] === 'v') {
		$version = substr($version, 1);
	}
	$dash = strpos($version, '-');
	if ($dash !== false) {
		$version = substr($version, 0, $dash);
	}
	$parts = explode('.', $version);
	while (count($parts) < 3) {
		$parts[] = '0';
	}
	$number = 0;
	for ($x = 0; $x < 3; $x++) {
		$val = intval($parts[$x]);
		if ($val >= 9990) {
			return $number * 10000 + 9999;
		} else if ($val > 99) {
			$val = 99;
		}
		$number = $number * 100 + $val;
	}
	return $number;
}

// Returns -1/0/1 comparing two FPP version strings (port of CompareFPPVersions).
function ComparePluginFPPVersions($a, $b)
{
	$a = PluginVersionToNumber($a);
	$b = PluginVersionToNumber($b);
	return ($a <=> $b);
}

/**
 * Get plugin information
 *
 * Get `pluginInfo.json` for installed plugin `{RepoName}`. An additional
 * `updatesAvailable` field indicates whether the plugin has commits that
 * have been fetched but not yet merged.
 *
 * @route GET /api/plugin/{RepoName}
 * @response 200 Plugin information
 * ```json
 * {
 *   "repoName": "fpp-matrixtools",
 *   "name": "MatrixTools",
 *   "author": "Chris Pinkham (CaptainMurdoch)",
 *   "srcURL": "https://github.com/cpinkham/fpp-matrixtools.git",
 *   "updatesAvailable": 0,
 *   "versions": [
 *     {
 *       "minFPPVersion": 0,
 *       "maxFPPVersion": 0,
 *       "branch": "master",
 *       "sha": ""
 *     }
 *   ]
 * }
 * ```
 */
function GetPluginInfo()
{
	global $settings;

	$plugin = params('RepoName');
	$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';

	if (file_exists($infoFile)) {
		$json = file_get_contents($infoFile);
		$result = json_decode($json, true);
		$result['Status'] = 'OK';
		$result['updatesAvailable'] = PluginHasUpdates($plugin);

		$iconFile = $settings['pluginDirectory'] . '/' . $plugin . '/icon.png';
		$result['hasIcon'] = file_exists($iconFile) || !empty($result['iconURL']);

		$pageInfo = _PluginGetBestPageUrl($plugin);
		$result['pageUrl'] = $pageInfo['url'];
		$result['pageType'] = $pageInfo['page'];

		return json($result);
	}

	$result = array();
	$result['Status'] = 'Error';

	if (!file_exists($settings['pluginDirectory'] . '/' . $plugin))
		$result['Message'] = 'Plugin is not installed';
	else
		$result['Message'] = 'pluginInfo.json does not exist';

	return json($result);
}

/**
 * Serve plugin icon
 *
 * Serves the plugin icon. First checks for a local icon.png in the plugin
 * directory. If not found, checks the plugin's pluginInfo.json for an
 * iconURL field and proxies it (same-origin, avoids CSP restrictions on
 * external image hosts).
 *
 * @route GET /api/plugin/{RepoName}/icon
 * @response 200 PNG image data
 * @response 404 No icon available
 */
function PluginServeIcon()
{
	global $settings;
	$plugin = params('RepoName');
	$pluginDir = $settings['pluginDirectory'] . '/' . $plugin;

	// Check for local icon.png
	$file = $pluginDir . '/icon.png';
	if (file_exists($file)) {
		$mtime = filemtime($file);
		$lastModified = gmdate('D, d M Y H:i:s', $mtime) . ' GMT';

		header('Content-Type: image/png');
		header('Cache-Control: public, max-age=0, must-revalidate');
		header('Last-Modified: ' . $lastModified);

		if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE']) && strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']) >= $mtime) {
			header('HTTP/1.1 304 Not Modified');
			exit;
		}

		ob_clean();
		flush();
		readfile($file);
		exit;
	}

	// Check pluginInfo.json for iconURL and proxy it
	$infoFile = $pluginDir . '/pluginInfo.json';
	if (file_exists($infoFile)) {
		$info = json_decode(file_get_contents($infoFile), true);
		if (!empty($info['iconURL'])) {
			$ctx = stream_context_create(['http' => ['timeout' => 10, 'follow_location' => 1, 'user_agent' => 'FPP']]);
			$data = @file_get_contents($info['iconURL'], false, $ctx);
			if ($data !== false) {
				header('Content-Type: image/png');
				header('Cache-Control: public, no-cache');
				echo $data;
				exit;
			}
		}
	}

	http_response_code(404);
}

/**
 * Uninstall plugin
 *
 * Uninstall plugin {RepoName}.
 *
 * @route DELETE /api/plugin/{RepoName}
 * @response 200 Plugin uninstalled
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function UninstallPlugin()
{
	global $settings, $fppDir, $SUDO, $_REQUEST;
	$result = array();
	$stream = $_REQUEST['stream'];

	$plugin = params('RepoName');

	if (file_exists($settings['pluginDirectory'] . '/' . $plugin)) {
		$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';
		if (file_exists($infoFile)) {
			$info = file_get_contents($infoFile);

			$data = json_decode($info, true);

			if (isset($data['linkName']))
				exec("rm " . $settings['pluginDirectory'] . "/" . $data['linkName'], $output, $return_val);

			// Drop this plugin's claim on any packages it declared as
			// dependencies. A package is only apt-removed once nothing else
			// (the user or another plugin) still requires it.
			if (isset($data['dependencies']['packages']) && is_array($data['dependencies']['packages'])) {
				if (isset($stream) && $stream != "false") {
					DisableOutputBuffering();
				}
				foreach ($data['dependencies']['packages'] as $pkg) {
					if (is_string($pkg) && $pkg !== '') {
						RemoveSystemPackageRequester($pkg, $plugin);
					}
				}
			}
		}

		if (isset($stream) && $stream != "false") {
			DisableOutputBuffering();
			system("$fppDir/scripts/uninstall_plugin $plugin", $return_val);
		} else {
			exec("export SUDO=\"" . $SUDO . "\"; export PLUGINDIR=\"" . $settings['pluginDirectory'] . "\"; $fppDir/scripts/uninstall_plugin $plugin", $output, $return_val);
			unset($output);
		}

		if ($return_val == 0) {
			if (isset($stream) && $stream != "false") {
				return "\nDone\n";
			}
			$result['Status'] = 'OK';
			$result['Message'] = '';
		} else {
			$result['Status'] = 'Error';
			$result['Message'] = 'Failed to properly uninstall plugin (' . $plugin . ')';
		}
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'The plugin (' . $plugin . ') is not installed';
	}

	return json($result);
}

/**
 * Check plugin for updates
 *
 * Check plugin `{RepoName}` for available updates by running `git fetch` in
 * the plugin directory and checking for any unmerged commits.
 *
 * @route POST /api/plugin/{RepoName}/updates
 * @response 200 Update check result
 * ```json
 * {"Status": "OK", "Message": "", "updatesAvailable": 1}
 * ```
 */
function CheckForPluginUpdates()
{
	global $settings, $SUDO;
	$result = array();

	$plugin = params('RepoName');

	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git fetch)';
	exec($cmd, $output, $return_val);

	if ($return_val == 0) {
		$result['Status'] = 'OK';
		$result['Message'] = '';
		$result['updatesAvailable'] = PluginHasUpdates($plugin);
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'Could not run git fetch for plugin ' . $plugin;
	}

	return json($result);
}

/**
 * Update plugin
 *
 * Pull in git updates for plugin `{RepoName}`. Supports an optional
 * `?stream=true` query parameter for streaming output.
 *
 * @route GET /api/plugin/{RepoName}/upgrade
 * @route POST /api/plugin/{RepoName}/upgrade
 * @response 200 Plugin upgraded
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function UpgradePlugin()
{
	global $settings, $SUDO, $_REQUEST, $fppDir;
	$result = array();

	$plugin = params('RepoName');
	$stream = $_REQUEST['stream'];

	// The git pull (plus its git-clean retry) and the plugin's optional
	// post-pull script (fpp_upgrade.sh, else fpp_install.sh -- for plugins
	// whose artifacts live outside git, e.g. prebuilt release binaries) all run
	// in scripts/upgrade_plugin. Like install_plugin / uninstall_plugin, that
	// wrapper logs (via startPluginLog) to the shared logs/fpp_plugin_manager.log, so a
	// failed upgrade is diagnosable from the log viewer / Support Zip instead of
	// the git-pull output vanishing. PLUGINDIR/SUDO are exported to match the values PHP
	// uses (the same way UninstallPlugin invokes uninstall_plugin).
	$cmd = 'export SUDO=' . escapeshellarg($SUDO)
		. '; export PLUGINDIR=' . escapeshellarg($settings['pluginDirectory'])
		. '; ' . escapeshellarg($fppDir . '/scripts/upgrade_plugin')
		. ' ' . escapeshellarg($plugin);

	if (isset($stream) && $stream != "false") {
		DisableOutputBuffering();
		system($cmd, $return_val);
		return "\nDone\n";
	}
	exec($cmd, $output, $return_val);

	if ($return_val == 0) {
		$result['Status'] = 'OK';
		$result['Message'] = '';
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'Could not run git pull for plugin ' . $plugin;
	}

	return json($result);
}

// Helper functions

/**
 * Injects GitHub credentials (username + Personal Access Token) into a
 * GitHub HTTPS URL so `git clone` and `curl` can authenticate against
 * private repositories.
 *
 * @param string $url GitHub HTTPS URL to inject credentials into.
 * @return string|false Modified URL on success, or false if credentials are not
 *                      configured or the URL is not a recognized GitHub URL.
 */
function InjectGitHubCredentials($url)
{
	global $settings;

	$user = isset($settings['gitHubUser']) ? trim($settings['gitHubUser']) : '';
	$pat = isset($settings['gitHubPAT']) ? trim($settings['gitHubPAT']) : '';

	if ($user === '' || $pat === '')
		return false;

	// Only inject into github.com / raw.githubusercontent.com URLs to avoid
	// leaking credentials to unrelated hosts.
	if (!preg_match('#^https://(github\.com|raw\.githubusercontent\.com|api\.github\.com)/#i', $url))
		return $url;

	return preg_replace('#^https://#i', 'https://' . rawurlencode($user) . ':' . rawurlencode($pat) . '@', $url, 1);
}

/**
 * Fetches the contents of a URL using GitHub credentials when available.
 * Falls back to `file_get_contents` when credentials are not configured.
 * `raw.githubusercontent.com` requires an `Authorization: token <PAT>`
 * header rather than HTTP Basic auth for private content. Temporary
 * share-link tokens (`?token=GHSAT...`) are stripped and replaced with
 * the configured PAT.
 *
 * @param string $url URL to fetch.
 * @return string|false Response body on success, or false on failure.
 */
function FetchURLWithGitHubCredentials($url)
{
	global $GitHubFetchLastError;
	$GitHubFetchLastError = '';
	global $settings;

	$user = isset($settings['gitHubUser']) ? trim($settings['gitHubUser']) : '';
	$pat = isset($settings['gitHubPAT']) ? trim($settings['gitHubPAT']) : '';
	$haveCreds = ($user !== '' && $pat !== '');

	// Only treat as a GitHub URL (and apply credentials/normalization) when
	// the host is one of the known GitHub hosts.
	$isGitHub = (bool) preg_match('#^https://(github\.com|raw\.githubusercontent\.com|api\.github\.com)/#i', $url);

	if ($isGitHub && $haveCreds) {
		// Strip GitHub's temporary "Raw" share-link token (e.g. ?token=GHSAT...)
		// since we'll authenticate with the configured PAT instead.
		$fetchUrl = preg_replace('/([?&])token=GHSAT[^&]*(&|$)/i', '$1', $url);
		$fetchUrl = preg_replace('/[?&]$/', '', $fetchUrl);
		$hadGhsatToken = ($fetchUrl !== $url);

		$attempts = array($fetchUrl);

		// Build a fallback URL using the GitHub Contents API which is the
		// most reliable way to fetch a file from a private repo with a PAT
		// (raw.githubusercontent.com can return 404 even with a valid PAT
		// in some configurations -- particularly with fine-grained tokens).
		if (preg_match('#^https://raw\.githubusercontent\.com/([^/]+)/([^/]+)/([^/]+)/(.+)$#i', $fetchUrl, $m)) {
			$owner = $m[1];
			$repo = $m[2];
			$ref = $m[3];
			$path = $m[4];
			// raw URLs sometimes use "refs/heads/<branch>"
			if (strpos($ref, 'refs') === 0 && isset($m[4])) {
				// path starts with "heads/<branch>/<file>" -- handle "refs/heads/<branch>/<path>"
				if (preg_match('#^heads/([^/]+)/(.+)$#', $path, $rm)) {
					$ref = $rm[1];
					$path = $rm[2];
				}
			}
			$apiUrl = 'https://api.github.com/repos/' . $owner . '/' . $repo . '/contents/' . $path . '?ref=' . rawurlencode($ref);
			$attempts[] = $apiUrl;
		}

		$lastCode = 0;
		$lastBody = '';
		$lastErr = '';
		foreach ($attempts as $tryUrl) {
			if (function_exists('curl_init')) {
				$ch = curl_init($tryUrl);
				curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
				curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
				curl_setopt($ch, CURLOPT_USERAGENT, 'FPP-PluginManager');
				curl_setopt($ch, CURLOPT_HTTPHEADER, array(
					'Authorization: token ' . $pat,
					'Accept: application/vnd.github.raw, application/json, */*',
					'X-GitHub-Api-Version: 2022-11-28',
				));
				$data = curl_exec($ch);
				$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
				$curlErr = curl_error($ch);
				curl_close($ch);
				if ($data !== false && $httpCode >= 200 && $httpCode < 400) {
					return $data;
				}
				$lastCode = $httpCode;
				$lastBody = is_string($data) ? $data : '';
				$lastErr = $curlErr;
			} else {
				$ctx = stream_context_create(array(
					'http' => array(
						'header' => "Authorization: token " . $pat . "\r\n" .
							"User-Agent: FPP-PluginManager\r\n" .
							"Accept: application/vnd.github.raw, application/json, */*\r\n",
						'follow_location' => 1,
						'ignore_errors' => 1,
					),
				));
				$data = @file_get_contents($tryUrl, false, $ctx);
				if ($data !== false && $data !== '') {
					return $data;
				}
				$lastBody = '';
				$lastErr = 'file_get_contents failed';
			}
		}

		// PAT authentication failed. If the original URL contained a GitHub share-link
		// token (?token=GHSAT...) try it as a last resort — server-side requests are not
		// subject to CORS, so the share-link token can work here even when the browser
		// fetch failed. This handles the case where the PAT is missing or expired but
		// the URL was freshly copied from GitHub.
		if ($hadGhsatToken) {
			$ghsatData = @file_get_contents($url);
			if ($ghsatData !== false && $ghsatData !== '') {
				return $ghsatData;
			}
		}

		$patHint = ($lastCode === 401 || $lastCode === 403)
			? ' The configured GitHub Personal Access Token may be invalid or expired — check the Developer settings page.'
			: '';
		$GitHubFetchLastError = 'HTTP ' . $lastCode .
			($lastErr !== '' ? ' (' . $lastErr . ')' : '') .
			($lastBody !== '' ? ': ' . trim(substr($lastBody, 0, 200)) : '') .
			$patHint;
		return false;
	}

	return @file_get_contents($url);
}

/**
 * Get plugin info from URL
 *
 * Server-side proxy for fetching a `pluginInfo.json` from a remote URL.
 * Used to retrieve plugin repository info without CORS issues, and to
 * authenticate against private GitHub repositories using credentials
 * configured on the Developer settings page.
 *
 * @route POST /api/plugin/fetchInfo
 * @body {"url": "https://example.com/pluginInfo.json", "useCredentials": 1}
 * @response 200 Plugin info fetched from remote URL
 * ```json
 * {}
 * ```
 */
// --- Plugin install-popularity proxy -------------------------------------
// Community install-count stats live on the SAME FPP stats server that FPP
// already submits anonymous usage stats to and checks for updates against:
// fppstats.falconchristmas.com (see statsPublishUrl in www/config.php and the
// fppstats.falconchristmas.com update checks in www/common.php). Using the
// FalconChristmas-branded hostname (a CNAME to the stats host) keeps us
// consistent with core FPP and off any contributor's personal domain. The
// device browser cannot fetch that host directly -- Apache's CSP connect-src
// blocks the cross-origin request -- so the Plugins UI calls this SAME-ORIGIN
// endpoint instead. We fetch the ~620KB summary server-side (CSP does not apply
// to PHP), requesting gzip so only ~65KB crosses the wire, then slim it to just
// the install counts (~3KB) and cache that on disk. Every browser/tab on this
// box shares one upstream fetch per TTL. Fail-soft: on any error we serve a
// stale cache if we have one, else an empty map -- the UI then hides the Popular
// strip and falls back to name sort (see BuildPopularStrip / GetPluginPopularity
// in plugins.php).
define('PLUGIN_POPULARITY_URL', 'https://fppstats.falconchristmas.com/api/summary/false');
define('PLUGIN_POPULARITY_PERIOD', 'last365Days');
define('PLUGIN_POPULARITY_TTL', 7 * 24 * 60 * 60); // 7d shared per-box cache (counts move slowly)

function PluginPopularityCacheFile()
{
	global $settings;
	$base = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : '/home/fpp/media';
	return $base . '/tmp/pluginPopularity.cache.json';
}

// Build the slim { period, counts } payload from a full stats-summary array.
// Coerces every count to a non-negative int (the feed is untrusted third-party
// data) and drops anything non-numeric.
function BuildSlimPluginPopularity($summary)
{
	if (!is_array($summary) || !isset($summary['topPlugins']['data'][PLUGIN_POPULARITY_PERIOD])) {
		return null;
	}
	$raw = $summary['topPlugins']['data'][PLUGIN_POPULARITY_PERIOD];
	if (!is_array($raw)) {
		return null;
	}
	$counts = array();
	foreach ($raw as $repo => $n) {
		if (is_string($repo) && $repo !== '' && is_numeric($n)) {
			$counts[$repo] = max(0, (int)$n);
		}
	}
	return array('period' => PLUGIN_POPULARITY_PERIOD, 'counts' => $counts);
}

/**
 * Get plugin install-popularity counts (repoName -> install count, last 365
 * days), proxied + cached from the community stats feed.
 *
 * @route GET /api/plugin/popularity
 * @response 200 { "period": "last365Days", "counts": { "remote-falcon": 1680 }, "source": "live|cache|stale|unavailable" }
 */
function GetPluginPopularity()
{
	$cacheFile = PluginPopularityCacheFile();

	// Fresh cache -> serve it, no upstream hit.
	if (file_exists($cacheFile) && (time() - filemtime($cacheFile)) < PLUGIN_POPULARITY_TTL) {
		$cached = json_decode(file_get_contents($cacheFile), true);
		if (is_array($cached) && isset($cached['counts'])) {
			$cached['source'] = 'cache';
			return json($cached);
		}
	}

	// Stale or missing cache -> fetch upstream (gzip), slim, refresh cache.
	$data = false;
	if (function_exists('curl_init')) {
		$ch = curl_init(PLUGIN_POPULARITY_URL);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
		curl_setopt($ch, CURLOPT_ENCODING, '');        // advertise gzip/deflate; curl auto-decodes
		curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 2);   // host down/unreachable -> give up fast (async, non-blocking)
		curl_setopt($ch, CURLOPT_TIMEOUT, 6);          // hard cap incl. transfer; never hang the page
		curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
		curl_setopt($ch, CURLOPT_USERAGENT, 'FPP-PluginPopularity');
		$data = curl_exec($ch);
		curl_close($ch);
	}

	$slim = ($data !== false && $data !== null && $data !== '')
		? BuildSlimPluginPopularity(json_decode($data, true))
		: null;

	if (is_array($slim)) {
		@file_put_contents($cacheFile, json_encode($slim));   // refresh shared cache
		$slim['source'] = 'live';
		return json($slim);
	}

	// Upstream failed. Serve a stale cache if present; else an empty map so the
	// UI hides the Popular strip rather than showing stale/absent data.
	if (file_exists($cacheFile)) {
		$cached = json_decode(file_get_contents($cacheFile), true);
		if (is_array($cached) && isset($cached['counts'])) {
			$cached['source'] = 'stale';
			return json($cached);
		}
	}
	return json(array('period' => PLUGIN_POPULARITY_PERIOD, 'counts' => new stdClass(), 'source' => 'unavailable'));
}

/**
 * Proxy-fetch a plugin icon image
 *
 * Fetches an image from an external URL and serves it with the correct
 * content-type. Used to bypass CSP restrictions that block loading
 * images from external hosts (e.g. raw.githubusercontent.com) directly
 * in `<img>` tags.
 *
 * @route GET /api/plugin/fetchImage?url=...
 * @response 200 Image data
 * @response 400 Missing or invalid URL
 */
function PluginFetchImage()
{
	$url = isset($_GET['url']) ? $_GET['url'] : '';
	if ($url === '' || !preg_match('#^https?://#i', $url)) {
		http_response_code(400);
		echo 'Missing or invalid url parameter';
		exit;
	}

	$ctx = stream_context_create(['http' => ['timeout' => 10, 'follow_location' => 1, 'user_agent' => 'FPP']]);
	$data = @file_get_contents($url, false, $ctx);
	if ($data === false) {
		http_response_code(404);
		exit;
	}

	// Determine content type from the URL extension
	$ext = strtolower(pathinfo(parse_url($url, PHP_URL_PATH), PATHINFO_EXTENSION));
	switch ($ext) {
		case 'png':  $ct = 'image/png'; break;
		case 'jpg':
		case 'jpeg': $ct = 'image/jpeg'; break;
		case 'gif':  $ct = 'image/gif'; break;
		case 'svg':  $ct = 'image/svg+xml'; break;
		case 'webp': $ct = 'image/webp'; break;
		default:     $ct = 'image/png';
	}

	header('Content-Type: ' . $ct);
	header('Cache-Control: public, max-age=86400');
	echo $data;
	exit;
}

function FetchPluginInfoProxy()
{
	$body = '';
	$fp = fopen('php://input', 'r');
	while ($d = fread($fp, 1024 * 16)) {
		$body .= $d;
	}
	fclose($fp);

	$req = json_decode($body, true);
	$url = isset($req['url']) ? $req['url'] : '';
	$useCreds = isset($req['useCredentials']) && $req['useCredentials'];

	if ($url === '' || !preg_match('#^https://#i', $url)) {
		return json(array('Status' => 'Error', 'Message' => 'Invalid URL'));
	}

	if ($useCreds) {
		$user = isset($GLOBALS['settings']['gitHubUser']) ? trim($GLOBALS['settings']['gitHubUser']) : '';
		$pat = isset($GLOBALS['settings']['gitHubPAT']) ? trim($GLOBALS['settings']['gitHubPAT']) : '';
		if ($user === '' || $pat === '') {
			return json(array('Status' => 'Error', 'Message' => 'GitHub user name and/or Personal Access Token are not configured on the Developer settings page.'));
		}
		$data = FetchURLWithGitHubCredentials($url);
	} else {
		$data = file_get_contents($url);
	}

	if ($data === false || $data === null || $data === '') {
		global $GitHubFetchLastError;
		$detail = (isset($GitHubFetchLastError) && $GitHubFetchLastError !== '') ? ' [' . $GitHubFetchLastError . ']' : '';
		return json(array('Status' => 'Error', 'Message' => 'Failed to fetch pluginInfo.json from ' . $url . $detail));
	}

	$decoded = json_decode($data, true);
	if (!is_array($decoded)) {
		$snippet = trim(substr($data, 0, 200));
		return json(array('Status' => 'Error', 'Message' => 'Response from ' . $url . ' was not valid JSON. First bytes: ' . $snippet));
	}

	return json($decoded);
}

/**
 * Checks whether the installed plugin has updates available: commits that
 * have been fetched but not yet merged into the local branch, or — for
 * plugins that distribute artifacts outside of git (e.g. prebuilt binaries
 * attached to a release) — updates reported by the plugin's own optional
 * update-check script.
 *
 * A plugin may provide scripts/fpp_update_check.sh. It is run with
 * FPPDIR/SRCDIR set (like fpp_install.sh); the last line of its stdout must
 * be "1" if an update is available or "0" if not. A non-zero exit status
 * means "could not check" and is ignored. The script's answer is OR'd with
 * the git check, so repo commits are still detected for such plugins.
 *
 * @param string $plugin Plugin directory name (repo name).
 * @return int 1 if updates are available, 0 otherwise.
 */
function PluginHasUpdates($plugin)
{
	global $settings, $fppDir;
	$output = '';

	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && git log $(git rev-parse --abbrev-ref HEAD)..origin/$(git rev-parse --abbrev-ref HEAD))';
	exec($cmd, $output, $return_val);

	if (($return_val == 0) && !empty($output))
		return 1;

	$check_script = $settings['pluginDirectory'] . '/' . $plugin . '/scripts/fpp_update_check.sh';
	if (file_exists($check_script)) {
		unset($output);
		exec("FPPDIR=" . $fppDir . " SRCDIR=" . $fppDir . "/src " . $check_script, $output, $return_val);
		if (($return_val == 0) && !empty($output) && (trim(end($output)) == '1'))
			return 1;
	}

	return 0;
}

/**
 * Get setting from plugin
 *
 * Returns the value of setting `{SettingName}` from plugin `{RepoName}`.
 *
 * @route GET /api/plugin/{RepoName}/settings/{SettingName}
 * @response 200 Plugin setting value
 * ```json
 * {"status": "OK", "SettingName": "SettingValue"}
 * ```
 */
function PluginGetSetting()
{
	$setting = params("SettingName");
	$plugin = params("RepoName");

	$value = ReadSettingFromFile($setting, $plugin);

	$result = array("status" => "OK");
	$result[$setting] = $value;

	return json($result);

}

/**
 * Set setting for plugin
 *
 * Sets `{SettingName}` for plugin `{RepoName}` and returns the updated value.
 *
 * @route POST /api/plugin/{RepoName}/settings/{SettingName}
 * @route PUT /api/plugin/{RepoName}/settings/{SettingName}
 * @body SettingValue
 * @response 200 Plugin setting updated
 * ```json
 * {"status": "OK", "SettingName": "SettingValue"}
 * ```
 */
function PluginSetSetting()
{

	$setting = params("SettingName");
	$plugin = params("RepoName");
	$value = file_get_contents('php://input');

	WriteSettingToFile($setting, $value, $plugin);

	return PluginGetSetting();
}

/**
 * Get plugin page URL
 *
 * Scans the plugin's menu files (menu.inc, status_menu.inc, etc.) and returns
 * the best page URL for the plugin.  Prefers the Status/Control page; falls
 * back to the Content Setup (config) page if no status page is found.
 *
 * @route GET /api/plugin/{RepoName}/page
 * @response 200 Plugin page info
 * ```json
 * {"url": "plugin.php?plugin=fpp-matrixtools&page=status.php", "page": "status.php", "found": true}
 * ```
 */
function GetPluginPageUrl()
{
	$plugin = params('RepoName');
	return json(_PluginGetBestPageUrl($plugin));
}

/**
 * Helper: compute the "best" page URL for a plugin by scanning its menu files.
 * Returns ['url' => ..., 'page' => ..., 'found' => true/false].
 *
 * Strategy – scan raw menu file content rather than PHP-including it, so we
 * are immune to PHP errors, missing variables, side effects, and unusual
 * execution contexts that would make @include produce empty or broken output:
 *
 *   1. Look for `'page' => 'xxx.php'` inside a PHP array (the template
 *      pattern: $menuEntries = [ ['page' => 'status.php', ...], ...]).
 *      When a `'type' => 'status'` / `'type' => 'content'` key precedes
 *      the page entry we can rank by menu section.
 *
 *   2. Look for `page=xxx.php` inside an href (old-style *_menu.inc or
 *      inline HTML in menu.inc).
 *
 *   3. Also try the PHP-include approach and merge results, so dynamically
 *      constructed URLs that cannot be found via raw-text scan are still
 *      captured (e.g. $page from a variable).  The raw-text scan runs
 *      first, so it wins for the common cases; the include outcome is a
 *      silent fallback that adds entries without replacing the raw ones.
 */
function _PluginGetBestPageUrl($plugin)
{
	global $pluginDirectory;
	$dir = $pluginDirectory . '/' . $plugin;

	// Gather candidate page names keyed by menu type (status, content, …).
	// _RawScan returns [pageName => true] for that menu type; a page that
	// appears under multiple types is recorded under each.
	$byType = [];

	// List of menu types in priority order (raw+include for each).
	foreach (['status', 'content', 'output', 'help'] as $type) {
		$names = _PluginScanMenuPagesRaw($dir, $plugin, $type);
		$byType[$type] = [];

		// Deduplicate while preserving the order they were found in.
		$seen = [];
		foreach ($names as $name) {
			if (!isset($seen[$name])) {
				$seen[$name] = true;
				$byType[$type][] = $name;
			}
		}
	}

	// Return the first page from the highest-priority menu type.
	foreach (['status', 'content', 'output', 'help'] as $type) {
		if (!empty($byType[$type])) {
			$page = $byType[$type][0];
			return [
				'url'   => 'plugin.php?plugin=' . urlencode($plugin) . '&page=' . urlencode($page),
				'page'  => $page,
				'found' => true,
			];
		}
	}

	return ['url' => null, 'page' => null, 'found' => false];
}

/**
 * Scan raw content of the plugin's menu files for a given menu type,
 * returning an array of page-file names (e.g. ['status.php', …]).
 *
 * Both file conventions are checked:
 *   – unified  menu.inc       (new pattern, PHP array-driven)
 *   – per-type  ${type}_menu.inc  (old pattern, static HTML)
 *
 * Inside each file two extractors run:
 *   a) PHP-include (fallback) – captures links constructed dynamically
 *      that a raw-text scan cannot see.
 *   b) Raw-text scan (primary) – immune to execution-context issues.
 */
function _PluginScanMenuPagesRaw($dir, $plugin, $type)
{
	$pages = [];
	$files = [];

	if (file_exists($dir . '/menu.inc')) {
		$files[] = ['path' => $dir . '/menu.inc', 'unified' => true];
	}
	$specific = $dir . '/' . $type . '_menu.inc';
	if (file_exists($specific)) {
		$files[] = ['path' => $specific, 'unified' => false];
	}

	foreach ($files as $info) {
		$path = $info['path'];

		// ---- (a) Raw-text scan (primary) --------------------------------
		// Runs first so its results take precedence over the include
		// fallback below: the raw scan reads the declared 'page' => value
		// directly and is immune to quirks in the rendered HTML (e.g. a
		// 'nopage=1' suffix whose "page=" substring can mislead the href
		// parser).
		$src = @file_get_contents($path);
		if ($src !== false && $src !== '') {
			_PluginExtractPageFromRaw($src, $info['unified'], $type, $pages);
		}

		// ---- (b) PHP-include fallback -----------------------------------
		// Executes the file with $menu/$plugin in scope so that PHP-built
		// hrefs are captured when the raw-text scan misses them. Failure
		// is silent. IMPORTANT: Save/restore $pages around the include
		// because the included file may define its own $pages variable
		// (e.g. with 'name', 'type', 'page' keys) which would corrupt our
		// array.
		$pagesBefore = $pages;
		$html = '';
		try {
			$menu = $type;
			ob_start();
			@include $path;
			$html = ob_get_clean();
		} catch (\Throwable $_) {
			$html = '';
		}
		$pages = $pagesBefore;
		if ($html !== '') {
			_PluginExtractPageFromHtml($html, $pages);
		}
	}

	return $pages;
}

/**
 * Extract page names from HTML output (the include fallback).
 *
 * Populates $pages (by reference) with page-file names found in
 * href="plugin.php?plugin=…&page=…" or href='…' attributes.
 */
function _PluginExtractPageFromHtml($html, array &$pages)
{
	// NOTE: anchor "page=" on a ?/& boundary so it does not match the "page="
	// substring inside "nopage=1" (added for menu entries with 'wrap' => 0).
	// Without the [?&] anchor the greedy [^"']* backtracks to the last "page="
	// and captures the nopage value (e.g. "1") instead of the real page name.
	if (preg_match_all('/href=(["\'])(?:[^"\']*plugin\.php[^"\']*[?&]page=([^"&\'&]+)|([^"\']*[?&]page=[^"&\'&]+[^"\']*plugin\.php[^"\']*))\1/i', $html, $m)) {
		foreach ($m[2] as $i => $page) {
			$v = trim($page !== '' ? $page : $m[3][$i]);
			if ($v !== '') {
				$pages[] = htmlspecialchars_decode($v);
			}
		}
	}
}

/**
 * Extract page names from raw PHP source code.
 *
 * Patterns (checked in order; first that matches wins):
 *
 *   1. PHP array entries:  'page' => 'xxx.php'
 *      When inside a unified menu.inc, also looks for a preceding
 *      'type' => '{type}' key to filter by the requested menu type.
 *
 *   2. page= in HTML attributes:  page="xxx.php"
 *
 *   3. page= in unquoted URL contexts:  page=xxx.php
 *
 *   4. Any .php filename inside quotes (broad catch-all for
 *      variable assignments like $statusPage = 'advancedstats.php').
 *
 *   5. Any .php filename that could be a page, even without
 *      surrounding quotes (aggressive fallback).
 *
 * Populates $pages (by reference).
 */
function _PluginExtractPageFromRaw($src, $unified, $type, array &$pages)
{
	$skipFiles = ['plugin.php'];

	// ---- Pattern 1: PHP array entries ----------------------------------
	$p1matchedAny = false;
	if ($unified && preg_match_all("/['\"]((?:type|page))['\"]\s*=>\s*['\"]([^'\"]+)['\"]/i", $src, $entryM, PREG_SET_ORDER)) {
		$lastType = null;
		foreach ($entryM as $em) {
			$key = strtolower($em[1]);
			$val = $em[2];
			$p1matchedAny = true;
			if ($key === 'type') {
				$lastType = $val;
			} elseif ($key === 'page' && preg_match('/\.php$/i', $val)) {
				if ($lastType === null || $lastType === $type) {
					$pages[] = $val;
				}
				$lastType = null;
			}
		}
		// If Pattern 1 found any array-style entries, stop here.
		// Even if none matched the requested type, we must not fall
		// through to the broader patterns which lack type filtering.
		if ($p1matchedAny) {
			return;
		}
	}

	// ---- Pattern 2: page= in HTML attributes ---------------------------
	if (preg_match_all('/page\s*=\s*["\']([a-zA-Z0-9_\-\.\/]+\.php)["\']/i', $src, $m)) {
		foreach ($m[1] as $v) {
			if (!in_array(strtolower($v), $skipFiles)) {
				$pages[] = $v;
			}
		}
		if (!empty($pages)) { return; }
	}

	// ---- Pattern 3: page= in unquoted URL contexts ---------------------
	if (preg_match_all('/page\s*=\s*([a-zA-Z0-9_\-\.\/]+\.php)/i', $src, $m)) {
		foreach ($m[1] as $v) {
			if (!in_array(strtolower($v), $skipFiles)) {
				$pages[] = $v;
			}
		}
		if (!empty($pages)) { return; }
	}

	// ---- Pattern 4: any .php filename in quotes (broad) ---------------
	if (preg_match_all('/["\']([a-zA-Z0-9_\-]+\.php)["\']/i', $src, $m)) {
		foreach ($m[1] as $v) {
			if (!in_array(strtolower($v), $skipFiles)) {
				$pages[] = $v;
			}
		}
		if (!empty($pages)) { return; }
	}

	// ---- Pattern 5: any .php filename after variable assignment -------
	if (preg_match_all('/=\s*["\']([a-zA-Z0-9_\-]+\.php)["\']/i', $src, $m)) {
		foreach ($m[1] as $v) {
			if (!in_array(strtolower($v), $skipFiles)) {
				$pages[] = $v;
			}
		}
	}
}

?>