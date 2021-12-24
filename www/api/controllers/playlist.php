<?

/////////////////////////////////////////////////////////////////////////////
function playlist_list()
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

function validatePlayListEntries(&$entries, &$media, &$playlist, &$rc)
{
    foreach ($entries as $e) {
        if ($e->type == "playlist") {
            if (property_exists($e, "name")) {
                if (!in_array($e->name, $playlist)) {
                    array_push($rc, "Invalid Playlist " . $e->name);
                }}
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

function playlist_list_validate()
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
        $pl = LoadPlayListDetails($plName, false);
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

        array_push($rc, array(
            "name" => $plName,
            "valid" => $valid,
            "messages" => $msg,
        )
        );
    }

    return json($rc);
}

function playlist_playable()
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

/////////////////////////////////////////////////////////////////////////////
// The sole purpose of this function is to ensure that media uploaded from
// xlights fppConnect removes special characters.  FPP UI only allows
// valid names to be selected, so less of a problem
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

/////////////////////////////////////////////////////////////////////////////
function playlist_insert()
{
    global $settings;

    $playlist = $GLOBALS['_POST'];
    $playlistName = $playlist['name'];

    cleanMedialNamesInPlaylist($playlist, "leadIn");
    cleanMedialNamesInPlaylist($playlist, "mainPlaylist");
    cleanMedialNamesInPlaylist($playlist, "leadOut");

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

    $json = json_encode($playlist/*, JSON_PRETTY_PRINT*/);

    $f = fopen($filename, "w");
    if ($f) {
        fwrite($f, $json);
        fclose($f);
    } else {
        $playlist['Status'] = 'Error';
        $playlist['Message'] = 'Unable to open file for writing';
    }

    return json($playlist);
}

/////////////////////////////////////////////////////////////////////////////
function LoadSubPlaylist(&$playlist, &$i, $plentry)
{
    $data = GetPlaylist($plentry->name);

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
            LoadSubPlaylist($subPlaylist, $li, $entry);
        }

        $li++;
    }

    array_splice($playlist, $i, 1, $subPlaylist);

    $i += count($subPlaylist) - 1;
}

function LoadPlayListDetails($file, $mergeSubs)
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

    $data = GetPlaylist($file);

    if (!$mergeSubs) {
        return $data;
    }

    if (isset($data->leadIn)) {
        $i = 0;
        foreach ($data->leadIn as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                LoadSubPlaylist($data->leadIn, $i, $entry);
            }
            $i++;
        }
    }

    if (isset($data->mainPlaylist)) {
        $i = 0;
        foreach ($data->mainPlaylist as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                LoadSubPlaylist($data->mainPlaylist, $i, $entry);
            }
            $i++;
        }
    }

    if (isset($data->leadOut)) {
        $i = 0;
        foreach ($data->leadOut as $entry) {
            if ($mergeSubs && $entry->type == "playlist") {
                LoadSubPlaylist($data->leadOut, $i, $entry);
            }
            $i++;
        }
    }

    return $data;
}

/////////////////////////////////////////////////////////////////////////////
function GetPlaylist($playlistName)
{
    global $settings;

    $jsonStr = file_get_contents($settings['playlistDirectory'] . '/' . $playlistName . ".json");

    return json_decode($jsonStr);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_get()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $mergeSubs = 0;
    if ((isset($_GET['mergeSubs'])) && ($_GET['mergeSubs'] == 1)) {
        $mergeSubs = 1;
    }

    $data = LoadPlayListDetails($playlistName, $mergeSubs);

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_update()
{
    global $settings;

    $playlist = $GLOBALS['_POST'];
    $playlistName = rawurldecode(params('PlaylistName'));

    cleanMedialNamesInPlaylist($playlist, "leadIn");
    cleanMedialNamesInPlaylist($playlist, "mainPlaylist");
    cleanMedialNamesInPlaylist($playlist, "leadOut");

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';

    $json = json_encode($playlist, JSON_PRETTY_PRINT);

    $f = fopen($filename, "w");
    if ($f) {
        fwrite($f, $json);
        fclose($f);
    } else {
        $playlist['Status'] = 'Error';
        $playlist['Message'] = 'Unable to open file for writing';
    }

    return json($playlist);
}

/////////////////////////////////////////////////////////////////////////////
function playlist_delete()
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
        }
    } else {
        $resp['Status'] = 'Error';
        $resp['Message'] = "Playlist '$playlistName' does not exist.";
    }

    return json($resp);
}

/////////////////////////////////////////////////////////////////////////////
function PlaylistSectionInsertItem()
{
    global $settings;

    $playlistName = rawurldecode(params('PlaylistName'));
    $sectionName = urldecode(params('SectionName'));
    $entry = $GLOBALS['_POST'];
    $resp = array();

    $filename = $settings['playlistDirectory'] . '/' . $playlistName . '.json';
    if (file_exists($filename)) {
        $json = file_get_contents($filename);
        $playlist = json_decode($json, true);

        if (!isset($playlist[$sectionName])) {
            $section = array();
            $playlist[$sectionName] = $section;
        }

        array_push($playlist[$sectionName], $entry);

        $json = json_encode($playlist, JSON_PRETTY_PRINT);

        $f = fopen($filename, "w");
        if ($f) {
            fwrite($f, $json);
            fclose($f);

            $resp['Status'] = 'OK';
            $resp['Message'] = '';
            $resp['playlistName'] = $playlistName;
            $resp['sectionName'] = $sectionName;
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

    $playlistName = rawurldecode(params('PlaylistName'));

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

    $playlistName = rawurldecode(params('PlaylistName'));
    $repeat = urldecode(params('Repeat'));

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
