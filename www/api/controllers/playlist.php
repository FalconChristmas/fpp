<?

/**
 * Get all playlists
 *
 * Get list of playlist names.
 *
 * @route GET /api/playlists
 * @response 200 List of playlist names
 * ```json
 * ["Playlist_1", "Playlist_2", "Playlist_3"]
 * ```
 */
function PlaylistList()
{
    global $settings;
    $playlists = array();
    if ($d = opendir($settings['playlistDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.json$/', $file)) {
                $file = preg_replace('/\.json$/', '', $file);
                array_push($playlists, $file);
            }
        }
        closedir($d);
    }
    sort($playlists);
    return json($playlists);
}

/**
 * Loads all media filenames from video, sequence, music, and image directories
 * for use in playlist validation.
 *
 * @return array Flat list of all media file names across all media directories.
 */
function loadValidateFiles()
{
    global $settings;
    $files = array();
    $types = array("videoDirectory", "sequenceDirectory", "musicDirectory", "imageDirectory");
    foreach ($types as $type) {
        if ($d = opendir($settings[$type])) {
            while (($file = readdir($d)) !== false) {
                array_push($files, $file);
            }
            closedir($d);
        }
    }
    return $files;
}

/**
 * Updates the `playlistInfo` array with section-level item counts and durations
 * in v4 format. Handles both v3 (backwards compatible) and v4 formats.
 *
 * @param array $playlist Playlist data array, modified in place.
 * @return void
 */
function updatePlaylistInfo(&$playlist)
{
    // Initialize playlistInfo if not present
    if (!isset($playlist['playlistInfo'])) {
        $playlist['playlistInfo'] = array();
    }

    // Set version to 4 if not set or if upgrading
    if (!isset($playlist['version']) || $playlist['version'] < 4) {
        $playlist['version'] = 4;
    }

    // Calculate section stats
    $leadInCount = isset($playlist['leadIn']) ? count($playlist['leadIn']) : 0;
    $mainCount = isset($playlist['mainPlaylist']) ? count($playlist['mainPlaylist']) : 0;
    $leadOutCount = isset($playlist['leadOut']) ? count($playlist['leadOut']) : 0;

    // Calculate durations from entries if they have duration field
    $leadInDuration = 0;
    if (isset($playlist['leadIn'])) {
        foreach ($playlist['leadIn'] as $entry) {
            if (isset($entry['duration']) && is_numeric($entry['duration'])) {
                $leadInDuration += floatval($entry['duration']);
            }
        }
    }

    $mainDuration = 0;
    if (isset($playlist['mainPlaylist'])) {
        foreach ($playlist['mainPlaylist'] as $entry) {
            if (isset($entry['duration']) && is_numeric($entry['duration'])) {
                $mainDuration += floatval($entry['duration']);
            }
        }
    }

    $leadOutDuration = 0;
    if (isset($playlist['leadOut'])) {
        foreach ($playlist['leadOut'] as $entry) {
            if (isset($entry['duration']) && is_numeric($entry['duration'])) {
                $leadOutDuration += floatval($entry['duration']);
            }
        }
    }

    // Update playlistInfo with v4 format
    $playlist['playlistInfo']['leadIn_items'] = $leadInCount;
    $playlist['playlistInfo']['leadIn_duration'] = $leadInDuration;
    $playlist['playlistInfo']['mainPlaylist_items'] = $mainCount;
    $playlist['playlistInfo']['mainPlaylist_duration'] = $mainDuration;
    $playlist['playlistInfo']['leadOut_items'] = $leadOutCount;
    $playlist['playlistInfo']['leadOut_duration'] = $leadOutDuration;
    $playlist['playlistInfo']['total_items'] = $leadInCount + $mainCount + $leadOutCount;
    $playlist['playlistInfo']['total_duration'] = $leadInDuration + $mainDuration + $leadOutDuration;
}

/**
 * Validates entries in a playlist section against known media files and
 * playlist names, appending error messages for any missing references.
 *
 * @param array $entries  Playlist section entries to validate.
 * @param array $media    List of known media file names.
 * @param array $playlist List of known playlist names.
 * @param array $rc       Error message array, modified in place.
 * @return void
 */
function validatePlayListEntries(&$entries, &$media, &$playlist, &$rc)
{
    foreach ($entries as $e) {
        if ($e->type == "playlist") {
            if (property_exists($e, "name")) {
                if (!in_array($e->name, $playlist)) {
                    array_push($rc, "Invalid Playlist " . $e->name);
                }
            }
        } else if ($e->type == "media") {
            if (property_exists($e, "mediaName")) {
                if (!in_array($e->mediaName, $media)) {
                    array_push($rc, "Invalid Playlist " . $e->mediaName);
                }
            }
        } else if ($e->type == "both") {
            if (property_exists($e, "mediaName")) {
                if (!in_array($e->mediaName, $media)) {
                    array_push($rc, "Invalid mediaName " . $e->mediaName);
                }
            }
            if (property_exists($e, "sequenceName")) {
                if (!in_array($e->sequenceName, $media)) {
                    array_push($rc, "Invalid Sequence " . $e->sequenceName);
                }
            }
        } else if ($e->type == "sequence" and property_exists($e, "sequenceName")) {
            if (!in_array($e->sequenceName, $media)) {
                array_push($rc, "Invalid Sequence " . $e->sequenceName);
            }
        } else if ($e->type == "image" and property_exists($e, "imagePath")) {
            if (!preg_match('/\/$/', $e->imagePath) && !in_array($e->imagePath, $media)) {
                array_push($rc, "Invalid Image " . $e->imagePath);
            }
        }
    }
}

/**
 * Validate all playlists
 *
 * Returns a list of all playlists with any validation errors, total item
 * counts, and total duration.
 *
 * @route GET /api/playlists/validate
 * @response 200 Validation results for all playlists
 * ```json
 * [
 *   {
 *     "name": "Test1",
 *     "description": "User entered playlist description",
 *     "valid": true,
 *     "messages": [],
 *     "total_duration": 10,
 *     "total_items": 3,
 *     "version": 4,
 *     "leadIn_items": 0,
 *     "mainPlaylist_items": 3,
 *     "leadOut_items": 0
 *   }
 * ]
 * ```
 */
function PlaylistListValidate()
{
    global $settings;
    $mediaFiles = loadValidateFiles();
    $playlists = array();
    if ($d = opendir($settings['playlistDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.json$/', $file)) {
                $file = preg_replace('/\.json$/', '', $file);
                array_push($playlists, $file);
            }
        }
        closedir($d);
    }
    sort($playlists);

    $rc = array();
    foreach ($playlists as $plName) {
        $pl = loadPlayListDetails($plName, false);
        $valid = true;
        $msg = [];
        if (isset($pl->leadIn)) {
            validatePlayListEntries($pl->leadIn, $mediaFiles, $playlists, $msg);
        }
        if (isset($pl->mainPlaylist)) {
            validatePlayListEntries($pl->mainPlaylist, $mediaFiles, $playlists, $msg);
        }
        if (isset($pl->leadOut)) {
            validatePlayListEntries($pl->leadOut, $mediaFiles, $playlists, $msg);
        }
        if (count($msg) > 0) {
            $valid = false;
        }

        //print_r($pl);
        if (isset($pl->desc)) {
            $plDesc = $pl->desc;
        } else {
            $plDesc = "";
        }
        if (isset($pl->playlistInfo->total_duration)) {
            $plDuration = $pl->playlistInfo->total_duration;
        } else {
            $plDuration = "";
        }
        if (isset($pl->playlistInfo->total_items)) {
            $plItems = $pl->playlistInfo->total_items;
        } else {
            $plItems = "";
        }

        // v4 format: include per-section stats
        $version = isset($pl->version) ? $pl->version : 3;
        $leadInItems = isset($pl->playlistInfo->leadIn_items) ? $pl->playlistInfo->leadIn_items : 0;
        $mainItems = isset($pl->playlistInfo->mainPlaylist_items) ? $pl->playlistInfo->mainPlaylist_items : 0;
        $leadOutItems = isset($pl->playlistInfo->leadOut_items) ? $pl->playlistInfo->leadOut_items : 0;

        array_push(
            $rc,
            array(
                "name" => $plName,
                "description" => $plDesc,
                "valid" => $valid,
                "messages" => $msg,
                "total_duration" => $plDuration,
                "total_items" => $plItems,
                "version" => $version,
                "leadIn_items" => $leadInItems,
                "mainPlaylist_items" => $mainItems,
                "leadOut_items" => $leadOutItems
            )
        );
    }

    return json($rc);
}

/**
 * Get playable objects
 *
 * Get a combined list of playlist names and `*.fseq` sequence filenames that are playable.
 *
 * @route GET /api/playlists/playable
 * @response 200 Playable playlist and sequence names
 * ```json
 * ["Playlist_1", "Playlist_2", "MySequence.fseq"]
 * ```
 */
function PlaylistPlayable()
{
    global $settings;
    $playlists = array();
    if ($d = opendir($settings['playlistDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.json$/', $file)) {
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

/**
 * Sanitizes media filenames within a playlist section to remove special
 * characters introduced by xLights fppConnect uploads. The FPP UI only allows
 * valid names to be selected, so this is less of a concern for normal use.
 *
 * @param array  $playlistObj Playlist data array, modified in place.
 * @param string $section     Section name to clean (e.g. "leadIn", "mainPlaylist").
 * @return void
 */
function cleanMedialNamesInPlaylist(&$playlistObj, $section)
{
    global $settings;
    $mapping = array(
        "sequenceName" => array("sequenceDirectory"),
        "mediaName" => array("musicDirectory", "videoDirectory"),
        "imagePath" => array("imageDirectory"),
    );

    foreach ($mapping as $type => $dirs) {
        foreach ($dirs as $dir) {
            if (isset($playlistObj[$section])) {
                foreach ($playlistObj[$section] as &$rec) {
                    if (isset($rec[$type])) {
                        $f = $settings[$dir] . "/" . $rec[$type];
                        if (!file_exists($f)) {
                            $f = $settings[$dir] . "/" . sanitizeFilename($rec[$type]);
                            if (file_exists($f)) {
                                $rec[$type] = sanitizeFilename($rec[$type]);
                            }
                        }
                    }
                }
            }
        }
    }

}

/**
 * Create playlist
 *
 * Insert a new playlist.
 *
 * @route POST /api/playlists
 * @body {"name": "UploadTest", "globalPauseBetweenSequencesMS": 5000, "mainPlaylist": [{"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}], "playlistInfo": {"total_duration": 8, "total_items": 1}}
 * @response 200 Newly created playlist
 * ```json
 * {
 *   "name": "UploadTest",
 *   "globalPauseBetweenSequencesMS": 5000,
 *   "mainPlaylist": [
 *     {"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}
 *   ],
 *   "playlistInfo": {
 *     "total_duration": 8,
 *     "total_items": 1
 *   }
 * }
 * ```
 */
function PlaylistInsert()
{
    global $settings;

    $playlist = $GLOBALS['_POST'];
    $playlistName = $playlist['name'];

    cleanMedialNamesInPlaylist($playlist, "leadIn");
    cleanMedialNamesInPlaylist($playlist, "mainPlaylist");
    cleanMedialNamesInPlaylist($playlist, "leadOut");

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

    $json = json_encode($playlist/*, JSON_PRETTY_PRINT*/);

    $f = file_put_contents($filename, $json, LOCK_EX);
    if ($f != false) {

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Playlist ' . $playlistName . ' was created.');
    } else {
        $playlist['Status'] = 'Error';
        $playlist['Message'] = 'Unable to open file for writing';
    }

    return json($playlist);
}

/**
 * Recursively loads and splices a sub-playlist's entries in place of a
 * `playlist`-type entry within a parent section array.
 *
 * @param array  $playlist Playlist section array being built, modified in place.
 * @param int    $i        Current index of the sub-playlist entry, updated in place.
 * @param object $plentry  Playlist entry object containing the sub-playlist name.
 * @return void
 */
function loadSubPlaylist(&$playlist, &$i, $plentry)
{
    $data = getPlaylist($plentry->name);

    $subPlaylist = array();

    if (isset($data->leadIn)) {
        $subPlaylist = array_merge($subPlaylist, $data->leadIn);
    }

    if (isset($data->mainPlaylist)) {
        $subPlaylist = array_merge($subPlaylist, $data->mainPlaylist);
    }

    if (isset($data->leadOut)) {
        $subPlaylist = array_merge($subPlaylist, $data->leadOut);
    }

    $li = 0;
    foreach ($subPlaylist as $entry) {
        if ($entry->type == "playlist") {
            loadSubPlaylist($subPlaylist, $li, $entry);
        }

        $li++;
    }

    array_splice($playlist, $i, 1, $subPlaylist);

    $i += count($subPlaylist) - 1;
}

/**
 * Loads a playlist's full details from disk, optionally merging all
 * sub-playlist entries recursively into the parent sections.
 *
 * @param string $file      Playlist name (without .json extension).
 * @param bool   $mergeSubs When true, recursively merges sub-playlists into parent sections.
 * @return object|string    Decoded playlist object, or empty string if the file does not exist.
 */
function loadPlayListDetails($file, $mergeSubs)
{
    global $settings;

    $playListEntriesLeadIn = null;
    $playListEntriesMainPlaylist = null;
    $playListEntriesLeadOut = null;
    $playlistInfo = null;

    $jsonStr = "";

    if (!file_exists($settings['playlistDirectory'] . '/' . $file . ".json")) {
        return "";
    }

    $data = getPlaylist($file);

    if (!$mergeSubs) {
        return $data;
    }

    if (isset($data->leadIn)) {
        $i = 0;
        foreach ($data->leadIn as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                loadSubPlaylist($data->leadIn, $i, $entry);
            }
            $i++;
        }
    }

    if (isset($data->mainPlaylist)) {
        $i = 0;
        foreach ($data->mainPlaylist as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                loadSubPlaylist($data->mainPlaylist, $i, $entry);
            }
            $i++;
        }
    }

    if (isset($data->leadOut)) {
        $i = 0;
        foreach ($data->leadOut as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                loadSubPlaylist($data->leadOut, $i, $entry);
            }
            $i++;
        }
    }

    return $data;
}

/**
 * Reads a playlist `*.json` file from disk and returns the decoded object.
 *
 * @param string $playlistName Playlist name (without .json extension).
 * @return object Decoded playlist object.
 */
function getPlaylist($playlistName)
{
    global $settings;

    $jsonStr = file_get_contents($settings['playlistDirectory'] . '/' . $playlistName . ".json", LOCK_SH);

    return json_decode($jsonStr);
}

/**
 * Get a playlist
 *
 * Get the playlist named `{PlaylistName}` in FPP JSON format. If
 * `?mergeSubs=1` is specified, sub-playlists are recursively merged into
 * the parent sections.
 *
 * @route GET /api/playlist/{PlaylistName}
 * @param int mergeSubs Merge sub-playlsits recursively
 * @response 200 Playlist details
 * ```json
 * {
 *   "name": "UploadTest",
 *   "globalPauseBetweenSequencesMS": 5000,
 *   "mainPlaylist": [
 *     {"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}
 *   ],
 *   "playlistInfo": {
 *     "total_duration": 8,
 *     "total_items": 1
 *   }
 * }
 * ```
 */
function PlaylistGet()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $mergeSubs = 0;
    if ((isset($_GET['mergeSubs'])) && ($_GET['mergeSubs'] == 1)) {
        $mergeSubs = 1;
    }

    $data = loadPlayListDetails($playlistName, $mergeSubs);

    return json($data);
}

/**
 * Upsert playlist
 *
 * Update or Insert (upsert) the playlist named {PlaylistName}.
 *
 * @route POST /api/playlist/{PlaylistName}
 * @body {"name": "UploadTest", "globalPauseBetweenSequencesMS": 5000, "mainPlaylist": [{"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}], "playlistInfo": {"total_duration": 8, "total_items": 1}}
 * @response 200 Updated playlist
 * ```json
 * {
 *   "name": "UploadTest",
 *   "globalPauseBetweenSequencesMS": 5000,
 *   "mainPlaylist": [
 *     {"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}
 *   ],
 *   "playlistInfo": {
 *     "total_duration": 8,
 *     "total_items": 1
 *   }
 * }
 * ```
 */
function PlaylistUpdate()
{
    global $settings;

    $playlist = $GLOBALS['_POST'];
    $playlistName = rawurldecode(params('PlaylistName'));

    cleanMedialNamesInPlaylist($playlist, "leadIn");
    cleanMedialNamesInPlaylist($playlist, "mainPlaylist");
    cleanMedialNamesInPlaylist($playlist, "leadOut");

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

    // Preserve duration values from existing playlist file ONLY if not provided by frontend
    if (file_exists($filename)) {
        $existingJson = file_get_contents($filename, LOCK_SH);
        $existingPlaylist = json_decode($existingJson, true);
        if (isset($existingPlaylist['playlistInfo'])) {
            if (!isset($playlist['playlistInfo'])) {
                $playlist['playlistInfo'] = array();
            }
            // Only preserve if frontend didn't send a value or sent 0
            if (
                (!isset($playlist['playlistInfo']['leadIn_duration']) || $playlist['playlistInfo']['leadIn_duration'] == 0)
                && isset($existingPlaylist['playlistInfo']['leadIn_duration'])
            ) {
                $playlist['playlistInfo']['leadIn_duration'] = $existingPlaylist['playlistInfo']['leadIn_duration'];
            }
            if (
                (!isset($playlist['playlistInfo']['mainPlaylist_duration']) || $playlist['playlistInfo']['mainPlaylist_duration'] == 0)
                && isset($existingPlaylist['playlistInfo']['mainPlaylist_duration'])
            ) {
                $playlist['playlistInfo']['mainPlaylist_duration'] = $existingPlaylist['playlistInfo']['mainPlaylist_duration'];
            }
            if (
                (!isset($playlist['playlistInfo']['leadOut_duration']) || $playlist['playlistInfo']['leadOut_duration'] == 0)
                && isset($existingPlaylist['playlistInfo']['leadOut_duration'])
            ) {
                $playlist['playlistInfo']['leadOut_duration'] = $existingPlaylist['playlistInfo']['leadOut_duration'];
            }
        }
    }

    // Update playlistInfo to maintain v4 format with section-level stats
    updatePlaylistInfo($playlist);

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

    $json = json_encode($playlist, JSON_PRETTY_PRINT);
    $f = file_put_contents($filename, $json, LOCK_EX);
    if ($f != false) {
        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Playlist ' . $playlistName . ' was updated.');
    } else {
        $playlist['Status'] = 'Error';
        $playlist['Message'] = 'Unable to open file for writing';
    }

    return json($playlist);
}

/**
 * Delete playlist
 *
 * Delete the playlist named {PlaylistName}.
 *
 * @route DELETE /api/playlist/{PlaylistName}
 * @response 200 Playlist deleted
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistDelete()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $resp = array();

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';
    if (file_exists($filename)) {
        unlink($filename);
        if (file_exists($filename)) {
            $resp['Status'] = 'Error';
            $resp['Message'] = 'Unable to delete Playlist.';
        } else {
            $resp['Status'] = 'OK';
            $resp['Message'] = '';

            //Trigger a JSON Configuration Backup
            GenerateBackupViaAPI('Playlist ' . $playlistName . ' was deleted.');
        }
    } else {
        $resp['Status'] = 'Error';
        $resp['Message'] = "Playlist '$playlistName' does not exist.";
    }

    return json($resp);
}

/**
 * Insert section into playlist
 *
 * Insert an item into the `{SectionName}` section of playlist `{PlaylistName}`.
 *
 * @route POST /api/playlist/{PlaylistName}/{SectionName}/item
 * @body {"type": "pause", "enabled": 1, "playOnce": 0, "duration": 8}
 * @response 200 Item inserted
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistSectionInsertItem()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $sectionName = urldecode(params('SectionName'));
    $entry = $GLOBALS['_POST'];
    $resp = array();

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';
    if (file_exists($filename)) {
        $json = file_get_contents($filename, LOCK_SH);
        $playlist = json_decode($json, true);

        if (!isset($playlist[$sectionName])) {
            $section = array();
            $playlist[$sectionName] = $section;
        }

        array_push($playlist[$sectionName], $entry);

        // Update playlistInfo for v4 format
        updatePlaylistInfo($playlist);

        $json = json_encode($playlist, JSON_PRETTY_PRINT);

        $f = file_put_contents($filename, $json, LOCK_EX);
        if ($f != false) {
            $resp['Status'] = 'OK';
            $resp['Message'] = '';
            $resp['playlistName'] = $playlistName;
            $resp['sectionName'] = $sectionName;

            //Trigger a JSON Configuration Backup
            GenerateBackupViaAPI('Playlist ' . $playlistName . ' content was modified.');
        } else {
            $playlist['Status'] = 'Error';
            $playlist['Message'] = 'Unable to open file for writing';
        }
    } else {
        $resp['Status'] = 'Error';
        $resp['Message'] = 'Playlist does not exist.';
    }

    return json($resp);
}

/**
 * Stop playlist
 *
 * Immediately stop the currently running playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlists/stop
 * @response 200 Playlist stopped
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStop()
{
    global $settings;
    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, "http://localhost:32322/command/Stop%20Now");
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Gracefully stop playlist
 *
 * Gracefully stop the currently running playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlists/stopgracefully
 * @response 200 Graceful stop initiated
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStopGracefully()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Stop%20Gracefully');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Gracefully stop at end of loop
 *
 * Gracefully stop the currently running playlist after completion of the
 * current loop.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlists/stopgracefullyafterloop
 * @response 200 Stop after loop initiated
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStopGracefullyAfterLoop()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Stop%20Gracefully/true');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Start playlist
 *
 * Start the playlist named `{PlaylistName}`. The optional query parameter
 * `scheduleProtected` (`true`/`false`) prevents the scheduler from stopping
 * this playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlist/{PlaylistName}/start
 * @response 200 Playlist started
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStart()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $scheduleProtected = isset($_GET['scheduleProtected']) ? $_GET['scheduleProtected'] : 'false';

    $curl = curl_init('http://localhost:32322/command/Start%20Playlist/' . $playlistName . '/false/false/' . $scheduleProtected);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Start playlist on repeat
 *
 * Start the playlist named `{PlaylistName}` with repeat mode. The optional
 * query parameter `scheduleProtected` (`true`/`false`) prevents the
 * scheduler from stopping this playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlist/{PlaylistName}/start/{Repeat}
 * @param bool scheduleProtected Prevent schedule from stopping this playlist
 * @response 200 Playlist started
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStartRepeat()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $repeat = urldecode(params('Repeat'));
    $scheduleProtected = isset($_GET['scheduleProtected']) ? $_GET['scheduleProtected'] : 'false';

    $curl = curl_init('http://localhost:32322/command/Start%20Playlist/' . $playlistName . '/' . $repeat . '/false/' . $scheduleProtected);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Start playlist on repeat (alt)
 *
 * Start the playlist named `{PlaylistName}` with repeat mode and schedule
 * protection. When `{ScheduleProtected}` is `true`, the scheduler cannot
 * stop this playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlist/{PlaylistName}/start/{Repeat}/{ScheduleProtected}
 * @response 200 Playlist started
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistStartRepeatProtected()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $repeat = urldecode(params('Repeat'));
    $scheduleProtected = urldecode(params('ScheduleProtected'));

    $curl = curl_init('http://localhost:32322/command/Start%20Playlist/' . $playlistName . '/' . $repeat . '/false/' . $scheduleProtected);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Pause currently running playlist
 *
 * Pause the currently running playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlists/pause
 * @response 200 Playlist paused
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistPause()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Pause%20Playlist');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Resume paused playlist
 *
 * Resume a previously paused playlist.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/playlists/resume
 * @response 200 Playlist resumed
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PlaylistResume()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/command/Resume%20Playlist');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}
