<?

/////////////////////////////////////////////////////////////////////////////
function playlist_list()
{
	global $settings;

	$result = array();
	$playlists = array();

	$playlist = array();
	$playlist['PlaylistName'] = 'Some Playlist Name'; // Give us something to return for testing
	if ($d = opendir($settings['playlistDirectory']))
	{
		while (($file = readdir($d)) !== false)
		{
			if (preg_match('/\.json$/', $file))
			{
				$file = preg_replace('/\.json$/', '', $file);
				array_push($playlists, $file);
			}
		}
		closedir($d);
	}

	$result['Playlists'] = $playlists;

	return json($playlists);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_insert()
{
	global $settings;

	$playlist = $GLOBALS['_POST'];
	$playlistName = $playlist['name'];

	$filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

	$json = json_encode($playlist, JSON_PRETTY_PRINT);

	$f = fopen($filename, "w");
	if ($f)
	{
		fwrite($f, $json);
		fclose($f);
	}
	else
	{
		$playlist['Status'] = 'Error';
		$playlist['Message'] = 'Unable to open file for writing';
	}

	return json($playlist);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_get()
{
	global $settings;

	$playlistName = params('PlaylistName');
	$filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

	render_file($filename);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_update()
{
	global $settings;

	$playlist = $GLOBALS['_POST'];
	$playlistName = params('PlaylistName');

	$filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

	$json = json_encode($playlist, JSON_PRETTY_PRINT);

	$f = fopen($filename, "w");
	if ($f)
	{
		fwrite($f, $json);
		fclose($f);
	}
	else
	{
		$playlist['Status'] = 'Error';
		$playlist['Message'] = 'Unable to open file for writing';
	}

	return json($playlist);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_delete()
{
	global $settings;

	$playlistName = params('PlaylistName');
	$resp = array();

	$filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';
	if (file_exists($filename))
	{
		unlink($filename);
		if (file_exists($filename))
		{
			$resp['Status'] = 'Error';
			$resp['Message'] = 'Unable to delete Playlist.';
		}
		else
		{
			$resp['Status'] = 'OK';
			$resp['Message'] = '';
		}
	}
	else
	{
		$resp['Status'] = 'Error';
		$resp['Message'] = 'Playlist does not exist.';
	}

	return json($resp);
}

/////////////////////////////////////////////////////////////////////////////
function PlaylistSectionInsertItem()
{
	global $settings;

	$playlistName = params('PlaylistName');
	$sectionName = params('SectionName');
	$entry = $GLOBALS['_POST'];
	$resp = array();

	$filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';
	if (file_exists($filename))
	{
		$json = file_get_contents($filename);
		$playlist = json_decode($json, true);

		if (!isset($playlist[$sectionName]))
		{
			$section = array();
			$playlist[$sectionName] = $section;
		}

		array_push($playlist[$sectionName], $entry);

		$json = json_encode($playlist, JSON_PRETTY_PRINT);

		$f = fopen($filename, "w");
		if ($f)
		{
			fwrite($f, $json);
			fclose($f);

			$resp['Status'] = 'OK';
			$resp['Message'] = '';
			$resp['playlistName'] = $playlistName;
			$resp['sectionName'] = $sectionName;
		}
		else
		{
			$playlist['Status'] = 'Error';
			$playlist['Message'] = 'Unable to open file for writing';
		}
	}
	else
	{
		$resp['Status'] = 'Error';
		$resp['Message'] = 'Playlist does not exist.';
	}

	return json($resp);
}

/////////////////////////////////////////////////////////////////////////////
?>
