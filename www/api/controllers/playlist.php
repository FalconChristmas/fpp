<?

/////////////////////////////////////////////////////////////////////////////
function playlist_list()
{
	global $settings;
	$playlists = array();
	if ($d = opendir($settings['playlistDirectory'])) {
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
	sort($playlists);
	return json($playlists);
}
function playlist_playable()
{
    global $settings;
    $playlists = array();
    if ($d = opendir($settings['playlistDirectory'])) {
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
    sort($playlists);
    
    $sequences = array();
    if ($d = opendir($settings['sequenceDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.fseq$/', $file)) {
                array_push($sequences, $file);
            }
        }
        closedir($d);
    }
    sort($sequences);
    $playlists = array_merge($playlists, $sequences);
    
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

function playlist_stop()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Stop Now');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_stopgracefully()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Stop Gracefully');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_stopgracefullyafterloop()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Stop Gracefully/true');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_start()
{
    global $settings;

    $playlistName = params('PlaylistName');

    $curl = curl_init('http://localhost:32322/command/Start Playlist/' . $playlistName);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_start_repeat()
{
    global $settings;

    $playlistName = params('PlaylistName');
    $repeat = params('Repeat');

    $curl = curl_init('http://localhost:32322/command/Start Playlist/' . $playlistName . '/' . $repeat);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_pause()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Pause Playlist');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
function playlist_resume()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Resume Playlist');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/////////////////////////////////////////////////////////////////////////////
?>
