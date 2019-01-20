<?

function help_help()
{
	$endpoints = array(
		[ 'GET /help', 'This help page', '', '' ],
		[ 'GET /playlists', 'Get list of playlist names', '', '[ "Playlist_1", "Playlist_2", "Playlist_3" ]' ],
		[ 'POST /playlists', 'Insert a new playlist.', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /playlist/:PlaylistName', 'Get Playlist named :PlaylistName.  Returns a normal FPP playlist in JSON format, _without_ the "Playlist" wrapper as above.', '', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }' ],
		[ 'POST /playlist/:PlaylistName', 'Update/Insert the Playlist named :PlaylistName.', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }', '{ "Status": "OK", "Message": "" }' ],
		[ 'DELETE /playlist/:PlaylistName', 'Delete the Playlist named :PlaylistName', '', '{ "Status": "OK", "Message": "" }' ],
		[ 'POST /playlist/:PlaylistName/:SectionName/item', 'Insert an item into the :SectionName section of the playlist :PlaylistName', '{ "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 }', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /plugin', 'Get list of installed plugins', '', '{ "installedPlugins": [ "fpp-matrixtools" ] }' ],
		[ 'GET /plugin/:PluginRepoName', 'Get pluginInfo.json for installed plugin PluginRepoName', '', '{ "repoName": "fpp-matrixtools", "name": "MatrixTools", "author": "Chris Pinkham (CaptainMurdoch)", "description": "This plugin uses FPP\'s Pixel Overlay feature to allow displaying dynamic text on a matrix and drawing on a matrix using your mouse in the web UI.", "homeURL": "https://github.com/cpinkham/fpp-matrixtools", "srcURL": "https://github.com/cpinkham/fpp-matrixtools.git", "bugURL": "https://github.com/cpinkham/fpp-matrixtools/issues", "versions": [ { "minFPPVersion": 0, "maxFPPVersion": 0, "branch": "master", "sha": "" } ] }' ]
	);

	$h = "
<h2>FPP API Endpoints</h2>

<style>
td {
	vertical-align: top;
}

.endpoint {
	white-space: nowrap;
}
</style>

<table border=1 cellpadding=5 width='100%'>
<tr><th>Endpoint</th><th>Description</th><th>Input JSON</th><th>Output JSON</th></tr>
";

	foreach ($endpoints as $endpoint)
	{
		$input = $endpoint[2];
		if ($input != '')
		{
			$input = json_encode(json_decode($input, true), JSON_PRETTY_PRINT);
			$input = "<pre>$input</pre>\n";
		}
		else
		{
			$input = '&nbsp;';
		}

		$output = $endpoint[3];
		if ($output != '')
		{
			$output = json_encode(json_decode($output, true), JSON_PRETTY_PRINT);
			$output = "<pre>$output</pre>\n";
		}
		else
		{
			$output = '&nbsp;';
		}

		$h .= sprintf("<tr><td class='endpoint'>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
			$endpoint[0], $endpoint[1], $input, $output);
	}

	return $h;
}

?>
