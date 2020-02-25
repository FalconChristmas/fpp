<?

function help_help()
{
	$endpoints = array(
		[ 'GET /help', 'This help page', '', '' ],
		[ 'GET /configfile', 'Get list of config files in /home/fpp/media/config', '', '{ "Path": "", "ConfigFiles": [ "File1", "File2", "File3" ] }' ],
		[ 'GET /configfile/:SubDir', 'Get list of config files in /home/fpp/media/config/SubDir', '', '{ "Path": "SubDir", "ConfigFiles": [ "File1", "File2", "File3" ] }' ],
		[ 'GET /configfile/:FileName<br>GET /configfile/:SubDir/:FileName', 'Get contents of config file', '', 'Raw config file contents' ],
		[ 'POST /configfile/:FileName<br>POST /configfile/:SubDir/:FileName', 'Upload config file.  NOTE: Content-type must equal "text/html" even if content is not HTML.', 'Raw config file contents', '{ "Status": "OK", "Message": "" }' ],
		[ 'DELETE /configfile/:FileName<br>DELETE /configfile/:SubDir/:FileName', 'Delete config file.', '', '{ "Status": "OK", "Message": "" }' ],
        [ 'POST /email/configure', 'Configure outbound email using existing settings', '', '{ "Status": "OK", "Message": "" }' ],
        [ 'POST /email/test', 'Send test email using existing settings', '', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /files/:DirName', 'Get list of files in :DirName where :DirName is one of ("config", "effects", "events", "images", "logs", "music", "playlists", "plugins", "scripts", "sequences", "tmp", "upload", "videos").', '', '[ "PlayIntro.sh", "TestScript-02.sh", "TestScript.sh", "UserCallbackHook.sh", "fppdWatcher.sh", "fppdWatcherStart.sh" ]' ],
		[ 'GET /media', 'Get list of media files (includes both music and video files).', '', '[ "1min_720p29_2014-10-01.mp4", "30min_720p29_2014-08-08.mp3", "30min_720p29_2014-08-08.mp4", "30min_720p29_2014-08-08.ogg", "5min_720p29_2014-10-01.mp4", "Frosty.mp4" ]' ],
		[ 'GET /media/:MediaName/meta', 'Get meta data for a specific media file.', '', '{ "1min_720p29_2014-10-01.mp4": { "duration": 60.010666666667 } }' ],
		[ 'GET /playlists', 'Get list of playlist names', '', '[ "Playlist_1", "Playlist_2", "Playlist_3" ]' ],
        [ 'GET /playlists/stop', 'Immediately stop the currently running playlist', '', '' ],
        [ 'GET /playlists/stopgracefully', 'Gracefully stop the currently running playlist', '', '' ],
		[ 'POST /playlists', 'Insert a new playlist.', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /playlist/:PlaylistName', 'Get Playlist named :PlaylistName.  Returns a normal FPP playlist in JSON format.', '', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }' ],
        [ 'GET /playlist/:PlaylistName/start', 'Start the playlist named :PlaylistName.', '', '' ],
		[ 'POST /playlist/:PlaylistName', 'Update/Insert the Playlist named :PlaylistName.', '{ "name": "UploadTest", "mainPlaylist": [ { "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 } ], "playlistInfo": { "total_duration": 8, "total_items": 1 } }', '{ "Status": "OK", "Message": "" }' ],
		[ 'DELETE /playlist/:PlaylistName', 'Delete the Playlist named :PlaylistName', '', '{ "Status": "OK", "Message": "" }' ],
		[ 'POST /playlist/:PlaylistName/:SectionName/item', 'Insert an item into the :SectionName section of the playlist :PlaylistName', '{ "type": "pause", "enabled": 1, "playOnce": 0, "duration": 8 }', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /plugin', 'Get list of installed plugins', '', '[ "fpp-matrixtools", "fpp-vastfmt", "fpp-brightness" ]' ],
		[ 'POST /plugin', 'Install a new plugin.  The data passed in is a pluginInfo JSON structure with branch and sha values added to the top level to indicate which branch and sha to install.', '{ "repoName": "fpp-matrixtools", "name": "MatrixTools", "author": "Chris Pinkham (CaptainMurdoch)", "description": "MatrixTools description here.", "homeURL": "https://github.com/cpinkham/fpp-matrixtools", "srcURL": "https://github.com/cpinkham/fpp-matrixtools.git", "bugURL": "https://github.com/cpinkham/fpp-matrixtools/issues", "branch": "master", "sha": "", "versions": [ { "minFPPVersion": 0, "maxFPPVersion": 0, "branch": "master", "sha": "" } ] }', '{ "Status": "OK", "Message": "" }' ],
		[ 'GET /plugin/:PluginRepoName', 'Get pluginInfo.json for installed plugin PluginRepoName.  An additional "updatesAvailable" field is added to indicate whether the plugin has any updates available that have been fetched but not merged.', '', '{ "repoName": "fpp-matrixtools", "name": "MatrixTools", "author": "Chris Pinkham (CaptainMurdoch)", "description": "MatrixTools description here.", "homeURL": "https://github.com/cpinkham/fpp-matrixtools", "srcURL": "https://github.com/cpinkham/fpp-matrixtools.git", "bugURL": "https://github.com/cpinkham/fpp-matrixtools/issues", "updatesAvailable": 0, "versions": [ { "minFPPVersion": 0, "maxFPPVersion": 0, "branch": "master", "sha": "" } ] }' ],
		[ 'POST /plugin/:PluginRepoName/updates', 'Check plugin PluginRepoName for available updates.  this works by running a "git fetch" in the plugin directory and checking for any non-merged updates.', '', '{ "Status": "OK", "Message": "", "updatesAvailable": 1 }' ],
		[ 'POST /plugin/:PluginRepoName/upgrade', 'Pull in git updates for plugin PluginRepoName', '', '{ "Status": "OK", "Message": "" }' ],
		[ 'DELETE /plugin/:PluginRepoName', 'Delete plugin PluginRepoName', '', '{ "Status": "OK", "Message": "" }' ],
        [ 'GET /sequence', 'Get a list of all FSEQ files', '', '{"GreatestShow","StPatricksDay","Valentine"}' ],
        [ 'GET /sequence/:SequenceName', 'Get the FSEQ file for the named sequence', '', 'Raw FSEQ file'],
        [ 'GET /sequence/:SequenceName/meta', 'Get metadata from the FSEQ file for the named sequence', '', '{"Name":"GreatestShow.fseq","Version":"2.0","ID":"1553194098754908","StepTime":25,"NumFrames":10750,"MaxChannel":84992,"ChannelCount":84992}'],
        [ 'POST /sequence/:SequenceName', 'Upload a new FSEQ file', '', 'Raw FSEQ file'],
        [ 'DELETE /sequence/:SequenceName', 'Delete the named FSEQ file', '', ''],
        [ 'GET /settings', 'Get JSON list of settings.', '', '' ],
        [ 'GET /settings/:SettingName', 'Get info about a for a particular setting.', '', '' ],
        [ 'GET /settings/:SettingName/options', 'Get array of options for a particular setting.  This is currently only valid for options requiring a list of valid items and only for some of those which are used in the settings and playlist User Interfaces.', '', '{ "HDMI": "HDMI", "Disabled": "Disabled", "Matrix": "Matrix" }' ]
	);
    $fppEndpoints = array(
        [ 'GET /fppd/status', 'Gets the current status of the FPPD daemon', '', '{"current_playlist":{"count":"0","index":"0","playlist":"","type":""},"current_sequence":"","current_song":"","fppd":"running","mode":2,"mode_name":"player","next_playlist":{"playlist":"No playlist scheduled.","start_time":""},"repeat_mode":"0","seconds_played":"0","seconds_remaining":"0","status":0,"status_name":"idle","time":"Tue Apr 02 08:06:34 EDT 2019","time_elapsed":"00:00","time_remaining":"00:00","volume":0}'],
        [ 'GET /commands', 'Gets a JSON description of the commands', '', '[{"name" : "Next Playlist Item"}, {"name" : "Start Playlist", "args" : [ {"description" : "Playlist Name", "type" : "string"}]}]' ],
        [ 'GET /command/{COMMANDID}/arg1/arg2/...', 'Runs the given command', '', '' ],
        [ 'GET /models', 'Gets all of the Pixel Overlay Models', '', '[{"ChannelCount":6144,"Name":"Matrix","Orientation":"horizontal","StartChannel":1,"StartCorner":"TL","StrandsPerString":1,"StringCount":32}]'],
        [ 'GET /models/:ModelName', 'Gets a single Pixel Overlay Model', '' , '{"ChannelCount":6144,"Name":"Matrix","Orientation":"horizontal","StartChannel":1,"StartCorner":"TL","StrandsPerString":1,"StringCount":32}'],
        [ 'POST /models', 'Uploads a new model-overlays.json file', '{"models" : [ {"ChannelCount" : 6144,"Name" : "Matrix","Orientation" : "horizontal", "StartChannel" : 1,"StartCorner" : "TL","StrandsPerString" : 1,"StringCount" : 32}]}', 'OK'],
        [ 'GET /overlays/fonts', 'Gets a list of fonts that can be used on the overlay models', '', '["Courier","Courier-Bold","Courier-Oblique","Courier-BoldOblique","Helvetica","Helvetica-Bold","Helvetica-Oblique","Helvetica-BoldOblique","Helvetica-Narrow","Helvetica-Narrow-Oblique","Helvetica-Narrow-Bold","Helvetica-Narrow-BoldOblique","Times-Roman","Times-Bold","Times-Italic","Times-BoldItalic","Symbol"]'],
        [ 'GET /overlays/models', 'Gets a list of the Pixel Overlay Models and their state', '', '[{"ChannelCount":6144,"Name":"Matrix","Orientation":"horizontal","StartChannel":1,"StartCorner":"TL","StrandsPerString":1,"StringCount":32,"isActive":0}]' ],
        [ 'GET /overlays/model/:ModelName', 'Gets the given overlay model and it\'s state', '', '{"ChannelCount":6144,"Name":"Matrix","Orientation":"horizontal","StartChannel":1,"StartCorner":"TL","StrandsPerString":1,"StringCount":32,"isActive":0}'],
        [ 'GET /overlays/model/:ModelName/clear', 'Clears the given model', '', 'OK'],
        [ 'GET /overlays/model/:ModelName/data', 'Gets the current channel data for the model', '', '{"data":[0,0,0,0,0,0],"isLocked":false}'],
        [ 'PUT /overlays/model/:ModelName/state', 'Sets the state of the overlay model', '{"State": 1}', 'OK'],
        [ 'PUT /overlays/model/:ModelName/fill', 'Fills the entire overlay with the given color', '{"RGB": [255, 0, 0]}', 'OK'],
        [ 'PUT /overlays/model/:ModelName/pixel', 'Sets a specific pixel in the model to the given color', '{"X": 10, "Y": 12, "RGB": [255, 0, 0]}', 'OK'],
        [ 'PUT /overlays/model/:ModelName/text', 'Displays text on the overlay model', '{"Message": "Hello", "Position": "L2R", "Font": "Helvetica", "FontSize": 12, "AntiAlias": false, "PixelsPerSecond": 5, "Color": "#FF000", "AutoEnable": false}', 'OK'],
        [ 'GET /fppd/log', 'Gets the current log mask', '', '{"log":{"level":"info","mask":"channelout,command,control,e131bridge"},"message":"","respCode":200,"status":"OK"}' ],
        [ 'GET /fppd/playlists', 'Get the current playing playlists', '', '{"message":"","playlists":["Test"],"respCode":200,"status":"OK"}'],
        [ 'GET /fppd/e131stats', 'Gets the current bridge mode input statistics', '', ''],
        [ 'GET /fppd/multiSyncSystems', 'Gets the list of known FPP instances', '', ''],
        [ 'GET /fppd/version', 'Gets the current FPP version', '', '{"branch":"master","fppdAPI":"v1","message":"","respCode":200,"status":"OK","version":"2.x-292-g83816a39-dirty"}'],
        [ 'GET /fppd/volume', 'Gets the current output volume', '', '{"message":"","respCode":200,"status":"OK","volume":72}'],
        [ 'GET /fppd/testing', 'Gets the current test mode configuration', '', '{"config":{"channelSet":"1-1048576","channelSetType":"channelRange","colorPattern":"FF000000FF000000FF","cycleMS":1000,"enabled":1,"mode":"RGBChase","subMode":"RGBChase-RGB"},"message":"","respCode":200,"status":"OK"}'],
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

	foreach ($endpoints as $endpoint) {
		$input = $endpoint[2];
		if ($input == '') {
			$input = '&nbsp;';
		} else if (preg_match('/{/', $input)) {
			$input = json_encode(json_decode($input, true), JSON_PRETTY_PRINT);
			$input = "<pre>$input</pre>\n";
		}

		$output = $endpoint[3];
		if ($output == '') {
			$output = '&nbsp;';
		} else if (preg_match('/[\[{]/', $output)) {
			$output = json_encode(json_decode($output, true), JSON_PRETTY_PRINT);
			$output = "<pre>$output</pre>\n";
		}

        $desc = $endpoint[0];
        if ((preg_match('/^GET \//', $endpoint[0])) && (!preg_match('/:/', $endpoint[0]))) {
            $desc = sprintf("<a href='/api/%s' target='_blank'>%s</a>",
                preg_replace('/^GET \//', '', $endpoint[0]), $endpoint[0]);
        }

		$h .= sprintf("<tr><td class='endpoint'>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
			$desc, $endpoint[1], $input, $output);
	}
    $h .= "</table><br><hr>
    <h2>FPPD Daemon Endpoints - these require FPPD to be running or a timeout error will occur</h2>
    <table border=1 cellpadding=5 width='100%'>
    <tr><th>Endpoint</th><th>Description</th><th>Input JSON</th><th>Output JSON</th></tr>
    ";
    foreach ($fppEndpoints as $endpoint) {
        $input = $endpoint[2];
        if ($input == '') {
            $input = '&nbsp;';
        } else if (preg_match('/{/', $input)) {
            $input = json_encode(json_decode($input, true), JSON_PRETTY_PRINT);
            $input = "<pre>$input</pre>\n";
        }
        
        $output = $endpoint[3];
        if ($output == '') {
            $output = '&nbsp;';
        } else if (preg_match('/[\[{]/', $output)) {
            $output = json_encode(json_decode($output, true), JSON_PRETTY_PRINT);
            $output = "<pre>$output</pre>\n";
        }
        
        $h .= sprintf("<tr><td class='endpoint'>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                      $endpoint[0], $endpoint[1], $input, $output);
    }

    
	return $h;
}

?>
