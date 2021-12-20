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
// GET /file/:DirName/copy/:source/:dest
function files_copy()
{
    $filename = params("source");
    $newfilename = params("dest");
    $dir = params("DirName");

    $dir = GetDirSetting($dir);

    if ($dir == "") {
        return json(array("status" => "Invalid Directory"));
    }

    $result = array();

    if (copy($dir . '/' . $filename, $dir . '/' . $newfilename)) {
        $result['status'] = 'success';
    } else {
        $result['status'] = 'failure';
    }

    $result['original'] = $filename;
    $result['new'] = $newfilename;

    return json($result);
}

function files_rename()
{
    $filename = params("source");
    $newfilename = params("dest");
    $dir = params("DirName");
    $dir = GetDirSetting($dir);

    if ($dir == "") {
        return json(array("status" => "Invalid Directory"));
    }

    $result = array();

    if (rename($dir . '/' . $filename, $dir . '/' . $newfilename)) {
        $result['status'] = 'success';
    } else {
        $result['status'] = 'failure';
    }

    $result['original'] = $filename;
    $result['new'] = $newfilename;

    return json($result);
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

    // if ?nameOnly=1 was passed, then just array of names
    if (isset($_GET['nameOnly']) && ($_GET['nameOnly'] == '1')) {
        $rc = array();
        foreach (scandir($dirName) as $fileName) {
            if ($fileName != '.' && $fileName != '..') {
                array_push($rc, $fileName);
            }
        }
        if (strtolower(params("DirName")) == "logs") {
            array_push($rc, "/var/log/messages");
            array_push($rc, "/var/log/syslog");
        }
        return json($rc);
    }

    foreach (scandir($dirName) as $fileName) {
        if ($fileName != '.' && $fileName != '..') {
            GetFileInfo($files, $dirName, $fileName);
        }
    }

    if (strtolower(params("DirName")) == "logs") {
        if (file_exists("/var/log/messages")) {
            GetFileInfo($files, "", "/var/log/messages");
        }

        if (file_exists("/var/log/syslog")) {
            GetFileInfo($files, "", "/var/log/syslog");
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
        passthru('tail -' . $lines . ' ' . $dir . '/' . $filename);
    }
}

function MoveFile()
{
    global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory, $imageDirectory, $configDirectory, $SUDO;

    $file = params("fileName");

    // Fix double quote uploading by simply moving the file first, if we find it with URL encoding
    if (strstr($file, '"')) {
        if (!rename($uploadDirectory . "/" . preg_replace('/"/', '%22', $file), $uploadDirectory . "/" . $file)) {
            //Firefox and xLights will upload with " intact so if the rename doesn't work, it's OK
        }
    }

    if (! file_exists($uploadDirectory . "/" . $file)) {
        $tempFile = sanitizeFilename($file);
        if (file_exists($uploadDirectory . "/" . $tempFile)) {
            // was sanitized during upload process
            $file = $tempFile;
        }
    }

    $status = "OK";

    if (file_exists($uploadDirectory . "/" . $file)) {
        if (preg_match("/\.(fseq)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $sequenceDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move sequence file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/\.(fseq.gz)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $sequenceDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move sequence file";
                return json(array("status" => $status));
            }
            $nfile = $file;
            $nfile = str_replace('"', '\\"', $nfile);
            exec("$SUDO gunzip -f \"$sequenceDirectory/$nfile\"");
        } else if (preg_match("/\.(eseq)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $effectDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move effect file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/\.(mp4|mkv|avi|mov|mpg|mpeg)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $videoDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move video file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/\.(gif|jpg|jpeg|png)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $imageDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move image file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $file)) {
            // Get rid of any DOS newlines
            $contents = file_get_contents($uploadDirectory . "/" . $file);
            $contents = str_replace("\r", "", $contents);
            file_put_contents($uploadDirectory . "/" . $file, $contents);

            if (!rename($uploadDirectory . "/" . $file, $scriptDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move script file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/\.(mp3|ogg|m4a|wav|au|m4p|wma|flac)$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $musicDirectory . '/' . $file)) {
                $status = "ERROR: Couldn't move music file";
                return json(array("status" => $status));
            }
        } else if (preg_match("/eeprom\.bin$/i", $file)) {
            if (!rename($uploadDirectory . "/" . $file, $configDirectory . '/cape-eeprom.bin')) {
                $status = "ERROR: Couldn't move eeprom file";
                return json(array("status" => $status));
            }
        }
    } else {
        $status = "ERROR: Couldn't find file '" . $file . "' in upload directory";
    }
    return json(array("status" => $status));
}

/// GET /api/files/zip/:DirName
function GetZipDir()
{
    global $SUDO;
    global $settings;
    global $logDirectory;
    global $mediaDirectory;

    $dirName = params("DirName");
    if ($dirName != "Logs") {
        return json(array("status" => "Unsupported Directory"));
    }

    // Rest of this is only applicable for "Logs"

    // Re-format the file name
    $filename = tempnam("/tmp", "FPP_Logs");

    // Gather troubleshooting commands output
    $cmd = "php " . $settings['fppDir'] . "/www/troubleshootingText.php > " . $settings['mediaDirectory'] . "/logs/troubleshootingCommands.log";
    exec($cmd, $output, $return_val);
    unset($output);

    // Create the object
    $zip = new ZipArchive();
    if ($zip->open($filename, ZIPARCHIVE::CREATE) !== true) {
        exit("Cannot open '$filename'\n");
    }
    foreach (scandir($logDirectory) as $file) {
        if ($file == "." || $file == "..") {
            continue;
        }
        $zip->addFile($logDirectory . '/' . $file, "Logs/" . $file);
    }

    if (is_readable("/var/log/messages")) {
        $zip->addFile("/var/log/messages", "Logs/messages.log");
    }

    if (is_readable("/var/log/syslog")) {
        $zip->addFile("/var/log/syslog", "Logs/syslog.log");
    }

    $files = array(
        "channelmemorymaps",
        "channeloutputs",
        "channelremap",
        "config/channeloutputs.json",
        //new v2 config files
        "config/schedule.json",
        "config/outputprocessors.json",
        "config/co-other.json",
        "config/co-pixelStrings.json",
        "config/co-bbbStrings.json",
        "config/co-universes.json",
        "config/ci-universes.json",
        "config/model-overlays.json",
        //
        "pixelnetDMX",
        "settings",
        "universes",
    );

    foreach ($files as $file) {
        if (file_exists("$mediaDirectory/$file")) {
            $fileData = '';
            //Handle these files differently, as they are CSV or other, and not a ini or JSON file
            //ScrubFile assumes a INI file for files with the .json extension
            if (in_array($file, array('schedule', 'channelmemorymaps', 'channeloutputs', 'channelremap', 'universes'))) {
                $fileData = file_get_contents("$mediaDirectory/$file");
            } else {
                $fileData = ScrubFile("$mediaDirectory/$file");
            }
            $zip->addFromString("Config/$file", $fileData);
        }
    }

    // /root/.asoundrc is only readable by root, should use /etc/ version
    exec($SUDO . " cat /root/.asoundrc", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to read /root/.asoundrc");
    } else {
        $zip->addFromString("Config/asoundrc", implode("\n", $output) . "\n");
    }
    unset($output);

    exec("cat /proc/asound/cards", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to read alsa cards");
    } else {
        $zip->addFromString("Logs/asound/cards", implode("\n", $output) . "\n");
    }
    unset($output);

    exec("/usr/bin/git --work-tree=" . dirname(dirname(__FILE__)) . "/ status", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to get a git status for logs");
    } else {
        $zip->addFromString("Logs/git_status.txt", implode("\n", $output) . "\n");
    }
    unset($output);

    exec("/usr/bin/git --work-tree=" . dirname(dirname(__FILE__)) . "/ diff", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to get a git diff for logs");
    } else {
        $zip->addFromString("Logs/fpp_git.diff", implode("\n", $output) . "\n");
    }
    unset($output);

    $zip->close();

    $timestamp = gmdate('Ymd.Hi');

    header('Content-type: application/zip');
    header('Content-disposition: attachment;filename=FPP_Logs_' . $timestamp . '.zip');
    ob_clean();
    flush();
    readfile($filename);
    unlink($filename);
    exit();

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
