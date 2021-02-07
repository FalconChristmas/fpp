<?

function MapExtention($filename)
{
    if (preg_match("/\.(fseq|fseq.gz)$/i", $filename)) {
        return GetDirSetting("sequence");
    } else if (preg_match("/\.(p3|ogg|m4a|wav|au|m4p|wma|flac)$/i", $filename)) {
        return GetDirSetting("music");
    } else if (preg_match("/\.(mp4|mkv|avi|mov|mpg|mpeg)$/i", $filename)) {
        return GetDirSetting("video");
    } else if (preg_match("/\.(gif|jpg|jpeg|png)$/i", $filename)) {
        return GetDirSetting("images");
    } else if (preg_match("/\.(eseq)$/i", $filename)) {
        return GetDirSetting("effects");
    } else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $filename)) {
        return GetDirSetting("scripts");
    } else {
        return "";
    }
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/files/:DirName
function GetFiles()
{
    $files = array();

    $dirName = params("DirName");
    check($dirName, "dirName", __FUNCTION__);
    $dirName = GetDirSetting($dirName);
    if ($dirName == "") {
        return json(array("status" => "Invalid Directory"));
    }

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
    if (isset($_GET['nameOnly'])) {
        $nameOnly = $_GET['nameOnly'];
        if ($nameOnly == "1") {
            $rc = array();
            foreach ($files as $f) {
                array_push($rc, $f["name"]);
            }
            return json($rc);
        }
    }
    return json(array("status" => "ok", "files" => $files));
}

function GetFile()
{
    $dirName = params("DirName");
    $fileName = params("Name");
    $lines = -1;
    $play = 0;
    $attach = 0;

    if (isset($_GET['tail'])) {
        $lines = intval($_GET['tail']);
	}
	
    if (isset($_GET['play'])) {
        $play = intval($_GET['play']);
    }

    if (isset($_GET['attach'])) {
        $attach = intval($_GET['attach']);
    }

    GetFileImpl($dirName, $fileName, $lines, $play, $attach);
}

function GetFileImpl($dir, $filename, $lines, $play, $attach)
{
    $isImage = 0;
    $isLog = 0;
    if ($dir == 'Images') {
        $isImage = 1;
    } else if ($dir == "Logs") {
        $isLog = 1;
    }

    $dir = GetDirSetting($dir);

    if ($dir == "") {
        return;
    }

    if ($play) {
        if (preg_match('/mp3$/i', $filename)) {
            header('Content-type: audio/mp3');
        } else if (preg_match('/ogg$/i', $filename)) {
            header('Content-type: audio/ogg');
        } else if (preg_match('/flac$/i', $filename)) {
            header('Content-type: audio/flac');
        } else if (preg_match('/m4a$/i', $filename)) {
            header('Content-type: audio/mp4');
        } else if (preg_match('/mov$/i', $filename)) {
            header('Content-type: video/mov');
        } else if (preg_match('/mp4$/i', $filename)) {
            header('Content-type: video/mp4');
        } else if (preg_match('/wav$/i', $filename)) {
            header('Content-type: audio/wav');
        } else if (preg_match('/mpg$/i', $filename)) {
            header('Content-type: video/mpg');
        } else if (preg_match('/mpg$/i', $filename)) {
            header('Content-type: video/mpeg');
        } else if (preg_match('/mkv$/i', $filename)) {
            header('Content-type: video/mkv');
        } else if (preg_match('/avi$/i', $filename)) {
            header('Content-type: video/avi');
        }

    } else if ($isImage) {
        header('Content-type: ' . mime_content_type($dir . '/' . $filename));

        if ($attach == 1) {
            header('Content-disposition: attachment;filename="' . $filename . '"');
        }

    } else {
        header('Content-type: application/binary');
        header('Content-disposition: attachment;filename="' . $filename . '"');
    }

    if ($isLog && (substr($filename, 0, 9) == "/var/log/")) {
        $dir = "/var/log";
        $filename = basename($filename);
    }

    ob_clean();
    flush();
    if ($lines == -1) {
        readfile($dir . '/' . $filename);
    } else {
		echo("Lines = $lines");
        passthru('tail -' . $lines . ' ' . $dir . '/' . $filename);
    }
}

function DeleteFile()
{
    $status = "File not found";
    $dirName = params("DirName");
    $fileName = params("Name");

    $dir = GetDirSetting($dirName);
    $fullPath = "$dir/$fileName";

    if ($dir == "") {
        $status = "Invalid Directory";
    } else if (!file_exists($fullPath)) {
        $status = "File Not Found";
    } else {
        if (unlink($fullPath)) {
            $status = "OK";
        } else {
            $status = "Unable to delete file";
        }
    }

    return json(array("status" => $status, "file" => $fileName, "dir" => $dirName));
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

    if (strpos(strtolower($dirName), "music") !== false || strpos(strtolower($dirName), "video") !== false) {

        //Check the cache first
        $cache_duration = media_duration_cache($fileName, null, $filesize);
        //cache duration will be null if not in cache, then retrieve it
        if ($cache_duration == null) {
            //Include our getid3 library for media
            require_once '../lib/getid3/getid3.php';
            //Instantiate getID3 object
            $getID3 = new getID3;
            //Get the media duration from file
            $ThisFileInfo = $getID3->analyze($fileFullName);
            //cache it
            if (isset($ThisFileInfo['playtime_seconds'])) {
                media_duration_cache($fileName, $ThisFileInfo['playtime_seconds'], $filesize);
            }

        } else {
            $ThisFileInfo['playtime_seconds'] = $cache_duration;
        }

        if (isset($ThisFileInfo['playtime_seconds'])) {
            $current["playtimeSeconds"] = human_playtime($ThisFileInfo['playtime_seconds']);
        } else {
            $current["playtimeSeconds"] = "Unknown";
        }

    }
    array_push($list, $current);
}
