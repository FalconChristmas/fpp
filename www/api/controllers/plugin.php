<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/plugin
function GetInstalledPlugins()
{
	global $settings;
	$plugins = Array();

	$dir = $settings['pluginDirectory'];

	if ($dh = opendir($dir))
	{
		while (($file = readdir($dh)) !== false)
		{
			if ((!in_array($file, array('.', '..'))) &&
				(is_dir($dir . '/' . $file)) &&
				(file_exists($dir . '/' . $file . '/pluginInfo.json')))
			{
				array_push($plugins, $file);
			}
		}
	}

	return json($plugins);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin
function InstallPlugin()
{
	global $settings, $fppDir, $SUDO;
	$result = Array();

	$pluginInfoJSON = "";
	$postdata = fopen("php://input", "r");
	while ($data = fread($postdata, 1024*16)) {
		$pluginInfoJSON .= $data;
	}
	fclose($postdata);

	$pluginInfo = json_decode($pluginInfoJSON, true);

	$plugin = $pluginInfo['repoName'];
	$srcURL = $pluginInfo['srcURL'];
	$branch = $pluginInfo['branch'];
	$sha = $pluginInfo['sha'];
	$infoURL = $pluginInfo['infoURL'];

	if (!file_exists($settings['pluginDirectory'] . '/' . $plugin))
	{
		exec("export SUDO=\"" . $SUDO . "\"; export PLUGINDIR=\"" . $settings['pluginDirectory'] . "\"; $fppDir/scripts/install_plugin $plugin \"$srcURL\" \"$branch\" \"$sha\"", $output, $return_val);
		unset($output);

		if ($return_val == 0)
		{
			$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';
			if (!file_exists($infoFile))
			{
				// no pluginInfo.json in repository, install the one we
				// installed the plugin from
				$info = file_get_contents($infoURL);
				file_put_contents($infoFile, $info);

				$data = json_decode($info, true);

				if (isset($data['linkName']))
				{
					exec("cd " . $settings['pluginDirectory'] . " && ln -s " . $plugin . " " . $data['linkName'], $output, $return_val);
					unset($output);
				}
			}

			$result['Status'] = 'OK';
			$result['Message'] = '';
		}
		else
		{
			$result['Status'] = 'Error';
			$result['Message'] = 'Could not properly install plugin';
		}
	}
	else
	{
		$result['Status'] = 'Error';
		$result['Message'] = 'The (' . $plugin . ') plugin is already installed';
	}

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/plugin/:RepoName
function GetPluginInfo()
{
	global $settings;

	$plugin = params('RepoName');
	$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';

	if (file_exists($infoFile))
	{
		$json = file_get_contents($infoFile);
		$result = json_decode($json, true);
		$result['Status'] = 'OK';
		$result['updatesAvailable'] = PluginHasUpdates($plugin);

		return json($result);
	}

	$result = Array();
	$result['Status'] = 'Error';

	if (!file_exists($settings['pluginDirectory'] . '/' . $plugin))
		$result['Message'] = 'Plugin is not installed';
	else
		$result['Message'] = 'pluginInfo.json does not exist';

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// DELETE /api/plugin/:RepoName
function UninstallPlugin()
{
	global $settings, $fppDir, $SUDO;
	$result = Array();

	$plugin = params('RepoName');

	if (file_exists($settings['pluginDirectory'] . '/' . $plugin))
	{
		$infoFile = $settings['pluginDirectory'] . '/' . $plugin . '/pluginInfo.json';
		if (file_exists($infoFile))
		{
			$info = file_get_contents($infoFile);

			$data = json_decode($info, true);

			if (isset($data['linkName']))
				exec("rm " . $settings['pluginDirectory'] . "/" . $data['linkName'], $output, $return_val);
		}

		exec("export SUDO=\"" . $SUDO . "\"; export PLUGINDIR=\"" . $settings['pluginDirectory'] ."\"; $fppDir/scripts/uninstall_plugin $plugin", $output, $return_val);
		unset($output);

		if ($return_val == 0)
		{
			$result['Status'] = 'OK';
			$result['Message'] = '';
		}
		else
		{
			$result['Status'] = 'Error';
			$result['Message'] = 'Failed to properly uninstall plugin (' . $plugin . ')';
		}
	}
	else
	{
		$result['Status'] = 'Error';
		$result['Message'] = 'The plugin (' . $plugin . ') is not installed';
	}

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin/:RepoName/updates
function CheckForPluginUpdates()
{
	global $settings, $SUDO;
	$result = Array();

	$plugin = params('RepoName');

	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git fetch)';
	exec($cmd, $output, $return_val);

	if ($return_val == 0)
	{
		$result['Status'] = 'OK';
		$result['Message'] = '';
		$result['updatesAvailable'] = PluginHasUpdates($plugin);
	}
	else
	{
		$result['Status'] = 'Error';
		$result['Message'] = 'Could not run git fetch for plugin ' . $plugin;
	}

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin/:RepoName/upgrade
function UpgradePlugin()
{
	global $settings, $SUDO;
	$result = Array();

	$plugin = params('RepoName');

    $cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git pull)';
	exec($cmd, $output, $return_val);
    if ($return_val != 0) {
        $cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && ' . $SUDO . ' git clean -fd && ' . $SUDO . ' git pull)';
        exec($cmd, $output, $return_val);
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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Helper functions
function PluginHasUpdates($plugin)
{
	global $settings;
	$output = '';

	$cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && git log $(git rev-parse --abbrev-ref HEAD)..origin/$(git rev-parse --abbrev-ref HEAD))';
	exec($cmd, $output, $return_val);

	if (($return_val == 0) && !empty($output))
		return 1;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////


?>
