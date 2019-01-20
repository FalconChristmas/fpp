<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/plugin
function GetInstalledPlugins()
{
	global $settings;
	$result = Array();
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

	$result['installedPlugins'] = $plugins;

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin
function InstallPlugin()
{
	global $settings;

	$result = Array();
	$result['Status'] = 'Error';
	$result['Message'] = 'This endpoint is currently not implemented';

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
		return render_file($infoFile, true);

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
	global $settings;
	$result = Array();

	$plugin = params('RepoName');

	$result['Status'] = 'Error';
	$result['Message'] = 'This endpoint is currently not implemented';

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin/:RepoName/updates
function CheckForPluginUpdates()
{
	global $settings;

	$plugin = params('RepoName');

	$result = Array();
	$result['Status'] = 'Error';
	$result['Message'] = 'This endpoint is currently not implemented';

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/plugin/:RepoName/upgrade
function UpgradePlugin()
{
	global $settings;

	$plugin = params('RepoName');

	$result = Array();
	$result['Status'] = 'Error';
	$result['Message'] = 'This endpoint is currently not implemented';

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////

?>
