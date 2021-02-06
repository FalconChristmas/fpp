<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/files/:DirName
function GetFiles()
{
    global $settings;
    global $mediaDirectory;
	global $sequenceDirectory;
	global $musicDirectory;
	global $videoDirectory;
	global $effectDirectory;
	global $scriptDirectory;
	global $logDirectory;
	global $uploadDirectory;
	global $imageDirectory;
	global $docsDirectory;

    $files = array();

	$dirName = params("DirName");
	check($dirName, "dirName", __FUNCTION__);
	if ($dirName == "Sequences" || $dirName == "sequences")    { $dirName = $sequenceDirectory; }
	else if ($dirName == "Music" || $dirName == "music")       { $dirName = $musicDirectory; }
	else if ($dirName == "Videos" || $dirName == "videos")     { $dirName = $videoDirectory; }
	else if ($dirName == "Effects" || $dirName == "effects")   { $dirName = $effectDirectory; }
	else if ($dirName == "Scripts" || $dirName == "scripts")   { $dirName = $scriptDirectory; }
	else if ($dirName == "Logs" || $dirName == "logs")         { $dirName = $logDirectory; }
	else if ($dirName == "Uploads" || $dirName == "uploads")   { $dirName = $uploadDirectory; }
	else if ($dirName == "Images" || $dirName == "images")     { $dirName = $imageDirectory; }
	else if ($dirName == "Docs" || $dirName == "docs")         { $dirName = $docsDirectory; }
	else
		return json(array("status"=>"Invalid Directory"));

    

    foreach (scandir($dirName) as $fileName) {
        if ($fileName != '.' && $fileName != '..') {
            GetFileInfo($files, $dirName, $fileName);
        }
    }

    if ($dirName == "Logs") {
        if (file_exists("/var/log/messages")) {
            GetFileInfo($root, "", "/var/log/messages");
        }

        if (file_exists("/var/log/syslog")) {
            GetFileInfo($root, "", "/var/log/syslog");
        }

    }

    // if ?nameOnly=1 was passed, then just array of names
    $nameOnly = $_GET['nameOnly'];
    if ($nameOnly == "1") {
        $rc = array();
        foreach ($files as $f) {
            array_push($rc, $f["name"]);
        }
        return json($rc);
    }
    return json(array("status" => "ok", "files"=>$files));
}

function GetFileInfo(&$list, $dirName, $fileName)
{
    $fileFullName = $dirName . '/' . $fileName;
    $filesize = filesize($fileFullName);
    $current = array();
    $current["name"] = $fileName;
    $current["mtime"] = date('m/d/y  h:i A', filemtime($fileFullName));
    $current["sizeBytes"] = $filesize;
    $current["sizeHuman"] = human_filesize($fileFullName);

	if(strpos(strtolower($dirName),"music") !== FALSE || strpos(strtolower($dirName),"video") !== FALSE ){

		//Check the cache first
		$cache_duration = media_duration_cache($fileName, null, $filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
			require_once('../lib/getid3/getid3.php');
			//Instantiate getID3 object
			$getID3 = new getID3;
			//Get the media duration from file
			$ThisFileInfo = $getID3->analyze($fileFullName);
			//cache it
			if (isset($ThisFileInfo['playtime_seconds']))
				media_duration_cache($fileName, $ThisFileInfo['playtime_seconds'], $filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

        if (isset($ThisFileInfo['playtime_seconds']))
            $current["playtimeSeconds"] = human_playtime($ThisFileInfo['playtime_seconds']);
		else
        $current["playtimeSeconds"] ="Unknown";
    }
    array_push($list, $current);
}
