<?

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
	global $settings, $fppDir, $SUDO, $_REQUEST;
	$result = array();

	$pluginInfoJSON = "";
	$postdata = fopen("php://input", "r");
	while ($data = fread($postdata, 1024 * 16)) {
		$pluginInfoJSON .= $data;
	}
	fclose($postdata);

	$pluginInfo = json_decode($pluginInfoJSON, true);

	$plugin = escapeshellcmd($pluginInfo['repoName']);
	$srcURL = $pluginInfo['srcURL'];
	$branch = escapeshellcmd($pluginInfo['branch']);
	$sha = $pluginInfo['sha'];
	$infoURL = $pluginInfo['infoURL'];
	$useCredentials = isset($pluginInfo['useCredentials']) && $pluginInfo['useCredentials'];

	// Always try to inject GitHub credentials for github.com URLs when they
	// are configured, regardless of whether the plugin is flagged private.
	// This handles the case where a private repo is installed via a public
	// listing (or a pluginInfo.json that does not set "private": true).
	// injectGitHubCredentials only modifies github.com / raw.githubusercontent.com
	// URLs so credentials are never leaked to unrelated hosts.
	$injectedURL = injectGitHubCredentials($srcURL);
	if ($injectedURL !== false) {
		$srcURL = $injectedURL;
	} else if ($useCredentials) {
		// Credentials were explicitly required but are not configured.
		$result['Status'] = 'Error';
		$result['Message'] = 'Use Credentials was selected but GitHub user name and/or Personal Access Token are not configured on the Developer settings page.';
		return json($result);
	}

	$stream = $_REQUEST['stream'];

	if (!file_exists($settings['pluginDirectory'] . '/' . $plugin)) {
		$return_val = 0;
		if (isset($stream) && $stream != "false") {
			DisableOutputBuffering();
			system("$fppDir/scripts/install_plugin $plugin \"$srcURL\" \"$branch\" \"$sha\"", $return_val);
		} else {
			exec("export SUDO=\"" . $SUDO . "\"; export PLUGINDIR=\"" . $settings['pluginDirectory'] . "\"; $fppDir/scripts/install_plugin $plugin \"$srcURL\" \"$branch\" \"$sha\"", $output, $return_val);
			unset($output);
		}

		if ($return_val == 0) {
			$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';
			if (!file_exists($infoFile)) {
				// no pluginInfo.json in repository, install the one we
				// installed the plugin from. fetchURLWithGitHubCredentials
				// transparently falls back to anonymous fetch when GitHub
				// credentials are not configured.
				$info = fetchURLWithGitHubCredentials($infoURL);
				file_put_contents($infoFile, $info);

				$data = json_decode($info, true);

				if (isset($data['linkName'])) {
					exec("cd " . $settings['pluginDirectory'] . " && ln -s " . $plugin . " " . $data['linkName'], $output, $return_val);
					unset($output);
				}
			}

			if (isset($stream) && $stream != "false") {
				return "\nDone\n";
			}
			$result['Status'] = 'OK';
			$result['Message'] = '';
		} else {
			$result['Status'] = 'Error';
			$result['Message'] = 'Could not properly install plugin';
		}
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'The (' . $plugin . ') plugin is already installed';
	}

	return json($result);
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
		$result['updatesAvailable'] = pluginHasUpdates($plugin);

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
		$result['updatesAvailable'] = pluginHasUpdates($plugin);
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
 * @route POST /api/plugin/{RepoName}/upgrade
 * @param bool stream When `true`, stream the upgrade output to the response instead of buffering it
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

	if (isset($stream) && $stream != "false") {
		DisableOutputBuffering();
		$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git pull)';
		system($cmd, $return_val);
		if ($return_val != 0) {
			$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git clean -fd && ' . $SUDO . ' git pull)';
			system($cmd, $return_val);
		}
		$install_script = $settings['pluginDirectory'] . '/' . $plugin . '/scripts/fpp_install.sh';
		if (!file_exists($install_script)) {
			$install_script = $settings['pluginDirectory'] . '/' . $plugin . '/fpp_install.sh';
		}
		if (file_exists($install_script)) {
			echo "Running install script " . $install_script . "\n";
			system($SUDO . "  FPPDIR=" . $fppDir . " SRCDIR=" . $fppDir . "/src " . $install_script, $return_val);
		}
		return "\nDone\n";
	}
	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git pull)';
	exec($cmd, $output, $return_val);
	if ($return_val != 0) {
		$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git clean -fd && ' . $SUDO . ' git pull)';
		exec($cmd, $output, $return_val);
	}
	$install_script = $settings['pluginDirectory'] . '/' . $plugin . '/scripts/fpp_install.sh';
	if (!file_exists($install_script)) {
		$install_script = $settings['pluginDirectory'] . '/' . $plugin . '/fpp_install.sh';
	}
	if (file_exists($install_script)) {
		exec($SUDO . "  FPPDIR=" . $fppDir . " SRCDIR=" . $fppDir . "/src " . $install_script, $return_val);
	}

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
function injectGitHubCredentials($url)
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
function fetchURLWithGitHubCredentials($url)
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

		$GitHubFetchLastError = 'HTTP ' . $lastCode .
			($lastErr !== '' ? ' (' . $lastErr . ')' : '') .
			($lastBody !== '' ? ': ' . trim(substr($lastBody, 0, 200)) : '');
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
		$data = fetchURLWithGitHubCredentials($url);
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
 * Checks whether the installed plugin has commits that have been fetched
 * but not yet merged into the local branch.
 *
 * @param string $plugin Plugin directory name (repo name).
 * @return int 1 if updates are available, 0 otherwise.
 */
function pluginHasUpdates($plugin)
{
	global $settings;
	$output = '';

	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && git log $(git rev-parse --abbrev-ref HEAD)..origin/$(git rev-parse --abbrev-ref HEAD))';
	exec($cmd, $output, $return_val);

	if (($return_val == 0) && !empty($output))
		return 1;

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

?>
