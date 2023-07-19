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
                if (!preg_match("//u", $fileName)) {
                    $fileName = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $fileName);
                }
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
            if (!preg_match("//u", $fileName)) {
                $fileName = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $fileName);
            }
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

    if ($isLog && (substr($filename, 0, 9) == "_var_log_")) {
        $dir = "/var/log";
        $filename = substr($filename, 9);
    }

	if (!file_exists($dir . '/' . $filename)){
		echo "File does not exist.";
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
        } else if (preg_match('/aac$/i', $filename)) {
            header('Content-type: audio/aac');
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

    ob_clean();
    flush();
    if ($lines == -1) {
        readfile($dir . '/' . $filename);
    } else {
        passthru('tail -' . $lines . ' ' . $dir . '/' . $filename);
    }
}

function findFile($dir, $filename)
{
    if (file_exists($dir . "/" . $filename)) {
        return $filename;
    }
    if (file_exists($dir . "/" . urldecode($filename))) {
        return urldecode($filename);
    }
    $tempFile = sanitizeFilename($filename);
    if (file_exists($dir . "/" . $tempFile)) {
        return $tempFile;
    }
    if (file_exists($dir . "/" . urldecode($tempFile))) {
        return urldecode($tempFile);
    }
    if (!preg_match("//u", $filename)) {
        $tempFile = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $filename);
        if (file_exists($dir . "/" . $tempFile)) {
            return $tempFile;
        }
        if (file_exists($dir . "/" . urldecode($tempFile))) {
            return urldecode($tempFile);
        }
    }
    return $filename;
}

function MoveFile()
{
    global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory, $imageDirectory, $configDirectory, $SUDO;

    $file = findFile($uploadDirectory, params("fileName"));


    // Fix double quote uploading by simply moving the file first, if we find it with URL encoding
    if (strstr($file, '"')) {
        if (!rename($uploadDirectory . "/" . preg_replace('/"/', '%22', $file), $uploadDirectory . "/" . $file)) {
            //Firefox and xLights will upload with " intact so if the rename doesn't work, it's OK
        }
    }

    if (!file_exists($uploadDirectory . "/" . $file)) {
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
        } else if (preg_match("/\.(mp3|ogg|m4a|wav|au|m4p|wma|flac|aac)$/i", $file)) {
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

/// GET /api/files/zip/:DirNames
function GetZipDir()
{
    global $mediaDirectory;

    $dirNames = params("DirNames");
    $dirNameArray = explode(',', $dirNames);

    if (count($dirNameArray) == 1) {
        $zipName = $dirNames;
    } else {
        $zipName = "all";
    }

    // Re-format the file name
    $filename = tempnam("/tmp", "FPP_$zipName");

    // Create the object
    $zip = new ZipArchive();
    if ($zip->open($filename, ZIPARCHIVE::CREATE) !== true) {
        exit("Cannot open '$filename'\n");
    }

    foreach ($dirNameArray as $dirName) {

        //Logs and Config are special
        if (strtolower($dirName) == "logs") {
            ZipLogs($zip);
        } else if (strtolower($dirName) == "config") {
            ZipConfigs($zip);
        } else {
            $dir = GetDirSetting($dirName);
            if (file_exists($dir) && $dir != "") {
                ZipDirectory($zip, $dirName, $dir);
            } else {
                $zip->close();
                return json(array("status" => "Directory not found: Name:$dirName, Path:$dir"));
            }
        }
    }

    $zip->close();

    $timestamp = gmdate('Ymd.Hi');

    header('Content-type: application/zip');
    header('Content-disposition: attachment;filename=FPP_' . $zipName . '_' . $timestamp . '.zip');
    ob_clean();
    flush();
    readfile($filename);
    unlink($filename);
    exit();
}

function ZipLogs($zip)
{
    global $SUDO;
    global $settings;
    global $logDirectory;
    global $mediaDirectory;

    ZipConfigs($zip);

    $ignore_files = array(
        "git_branch.log", // No longer generated
        "git_checkout_version.log", // No longer generated
        "git_fetch.log", // No longer generated
    );

    // Gather troubleshooting commands output
    $cmd = "php " . $settings['fppDir'] . "/www/troubleshootingText.php > " . $settings['mediaDirectory'] . "/logs/troubleshootingCommands.log";
    exec($cmd, $output, $return_val);
    unset($output);

    foreach (scandir($logDirectory) as $file) {
        if ($file == "." || $file == ".." || in_array($file, $ignore_files)) {
            continue;
        }
        $zip->addFile($logDirectory . '/' . $file, "logs/" . $file);
    }

    if (is_readable("/var/log/messages")) {
        $zip->addFile("/var/log/messages", "logs/messages.log");
    }

    if (is_readable("/var/log/syslog")) {
        $zip->addFile("/var/log/syslog", "logs/syslog.log");
    }

    exec("cat /proc/asound/cards", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to read alsa cards");
    } else {
        $zip->addFromString("logs/asound/cards", implode("\n", $output) . "\n");
    }
    unset($output);

    exec("/usr/bin/git --work-tree=" . gitBaseDirectory() . "/ status", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to get a git status for logs");
    } else {
        $zip->addFromString("logs/git_status.txt", implode("\n", $output) . "\n");
    }
    unset($output);

    exec("/usr/bin/git --work-tree=" . gitBaseDirectory() . "/ diff", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to get a git diff for logs");
    } else {
        $zip->addFromString("logs/fpp_git.diff", implode("\n", $output) . "\n");
    }
    unset($output);
}

function ZipConfigs($zip)
{
    global $SUDO;
    global $mediaDirectory;

    $files = array(
        "channelmemorymaps",
        "channeloutputs",
        "channelremap",
        "config/channeloutputs.json",
        //new v2 config files
        "config/channeloutputs.json",
        "config/ci-universes.json",
        "config/co-bbbStrings.json",
        "config/commandPresets.json",
        "config/co-other.json",
        "config/co-pixelStrings.json",
        "config/co-universes.json",
        "config/instantCommand.json",
        "config/model-overlays.json",
        "config/outputprocessors.json",
        "config/schedule.json",
        //
        "pixelnetDMX",
        "settings",
        "universes",
        "fpp-info.json",
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
            $zip->addFromString("$file", $fileData);
        }
    }

    // /root/.asoundrc is only readable by root, should use /etc/ version
    exec($SUDO . " cat /root/.asoundrc", $output, $return_val);
    if ($return_val != 0) {
        error_log("Unable to read /root/.asoundrc");
    } else {
        $zip->addFromString("asoundrc", implode("\n", $output) . "\n");
    }
    unset($output);
}

function ZipDirectory($zip, $name, $directory)
{
    global $mediaDirectory;
    foreach (scandir($directory) as $file) {
        if ($file == "." || $file == "..") {
            continue;
        }
        $zip->addFile("$directory/$file", "$name/$file");
    }
}

function DeleteFile()
{
    $status = "File not found";
    $dirName = params("DirName");
    $dir = GetDirSetting($dirName);
    $fileName = findFile($dir, params("Name"));

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

// emulate large fseek($fp, $pos, SEEK_SET) on 32-bit system
function emulated_fseek_for_big_files($fp, $pos)
{
    if (bccomp((string) PHP_INT_MAX, $pos, 0) > -1) {
        // small, do it natively
        fseek($fp, (int) $pos, SEEK_SET);
    } else {
        // crossing the 2G file size on 32BIT triggers an fseek error
        // Thus, we'll go to the end, read through the 2G, and then
        // start seeking again.  However, beyond 2G, we can also
        // only seek in 8K chunks which is definitely slower. :(
        fseek($fp, PHP_INT_MAX, SEEK_SET);
        fread($fp, 1); // get past fseek limitation
        $pos = bcsub(bcsub($pos, (string) PHP_INT_MAX), 1);

        $chunk = 8192 - 1; // get around weird 8KB limitation
        while ($pos) {
            if (bccomp((string) $chunk, $pos, 0) > -1) {
                fseek($fp, (int) $pos, SEEK_CUR);
                break;
            } else {
                fseek($fp, $chunk, SEEK_CUR);
                fread($fp, 1);
                $pos = bcsub($pos, (string) ($chunk + 1));
            }
        }
    }
}

function PatchFile()
{
    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        return uniqid("", true);
    }
    $status = "OK";
    $dirName = params("DirName");
    $fileName = $_SERVER['HTTP_UPLOAD_NAME'];
    if (!preg_match("//u", $fileName)) {
        $fileName = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $fileName);
    }    
    $offset = $_SERVER['HTTP_UPLOAD_OFFSET'];
    $length = $_SERVER['HTTP_UPLOAD_LENGTH'];

    $dir = GetDirSetting($dirName);
    $fullPath = "$dir/$fileName";
    if ($offset == 0) {
        //for the first chunk, clear out any existing patches
        $patch = glob($fullPath . '.patch.*');
        foreach ($patch as $fn) {
            unlink($fn);
        }
    }

    $patch_handle = fopen('php://input', 'rb');
    if ($offset != 0 && file_exists($fullPath . '.patch.0')) {
        $fileLen = real_filesize($fullPath . ".patch.0");
        if (bccomp($fileLen, $offset) == 0) {
            //it's the next patch, we can append the data instead of
            //attempting to create a bunch of patch files to then
            //have to spend time coping over later
            file_put_contents($fullPath . '.patch.0', $patch_handle, FILE_APPEND | LOCK_EX);
        } else {
            file_put_contents($fullPath . '.patch.' . $offset, $patch_handle, LOCK_EX);
        }
    } else {
        file_put_contents($fullPath . '.patch.' . $offset, $patch_handle, LOCK_EX);
    }
    fclose($patch_handle);

    $size = 0;
    $patch = glob(escapeshellcmd($fullPath) . '.patch.*');
    foreach ($patch as $fn) {
        $fileLen = real_filesize($fn);
        $size = bcadd($size, $fileLen, 0);
    }
    if (bccomp($size, $length, 0) == 0) {
        $offsets = array();
        // write patches to file
        foreach ($patch as $fn) {
            // get offset from fn
            list($dir, $offset) = explode($fileName . '.patch.', $fn, 2);
            array_push($offsets, $offset);
        }
        //sort by offset so we can just continuously write and not have to seek
        usort($offsets, "bccomp");

        // create output file
        $file_handle = false;

        // write patches to file
        foreach ($offsets as $offset) {
            // apply patch
            if (bccomp($offset, 0) == 0) {
                //first chunk, we can rename it instead of copying the data
                $ok = unlink($fullPath);
                $ok = rename($fullPath . '.patch.' . $offset, $fullPath);
                $file_handle = fopen($fullPath, 'a');
            } else {
                $patch_handle = fopen($fullPath . '.patch.' . $offset, 'r');
                while (!feof($patch_handle)) {
                    $read = fread($patch_handle, 64 * 1024);
                    fwrite($file_handle, $read);
                }
                fclose($patch_handle);
                unlink($fullPath . '.patch.' . $offset);
            }
        }

        // done with file
        fclose($file_handle);
    }
    return json(array("status" => $status, "file" => $fileName, "dir" => $dirName, "size" => $size));
}

function PostFile()
{
    $status = "OK";
    $dirName = params("DirName");
    $fileName = params("Name");

    $dir = GetDirSetting($dirName);
    $fullPath = "$dir/$fileName";
    $size = 0;
    $totalSize = 0;
    $offset = 0;
    if ($dir == "") {
        $status = "Invalid Directory";
    } else {
        $fpIn = fopen("php://input", 'rb');
        $mode = "w";
        if (isset($_REQUEST["sb"])) {
            $mode = "c+";
        }
        $fpOut = fopen($fullPath, $mode);

        flock($fpOut, LOCK_EX);

        fseek($fpOut, 0, SEEK_SET);
        if (isset($_REQUEST["sb"])) {
            // these parameters cannot be >2GB on 32bit systems so using startBlock * blockSize to
            // move beyond 2GB limit with SEEK_CUR
            $startBlock = isset($_REQUEST["sb"]) ? $_REQUEST["sb"] : 0;
            $blockSize = isset($_REQUEST["bs"]) ? $_REQUEST["bs"] : 0;
            $offset = bcmul($startBlock, $blockSize, 0);
            if ($offset > 0) {
                emulated_fseek_for_big_files($fpOut, $offset);
                $totalSize = bcadd($totalSize, $offset, 0);
            }
        }
        while (!feof($fpIn)) {
            $read = fread($fpIn, 64 * 1024); //64K blocks, (in actuality, Apache will likely send in 16K blocks)
            $written = fwrite($fpOut, $read);
            $size = bcadd($size, $written, 0);
        }
        $totalSize = bcadd($totalSize, $size, 0);

        fflush($fpOut);
        flock($fpOut, LOCK_UN);
        fclose($fpOut);
        fclose($fpIn);
    }
    //error_log("Done Uploading file " . $fullPath . "   Status: " . $status . "    Size: " . $totalSize . "   Written: " . $size . "   Offset: " . $offset);
    return json(array("status" => $status, "file" => $fileName, "dir" => $dirName, "written" => $size, "size" => $totalSize, "offset" => $offset));
}

function GetFileInfo(&$list, $dirName, $fileName)
{
    $fileFullName = $dirName . '/' . $fileName;
    $filesize = real_filesize($fileFullName);
    if (intval($filesize) < PHP_INT_MAX) {
        $filesize = intval($filesize);
    }
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

            $resp = GetMetaDataFromFFProbe($fileName);

            //cache it
            if (isset($resp['format']['duration'])) {
                media_duration_cache($fileName, $resp['format']['duration'], $filesize);
            }

        } else {
            $resp['format']['duration'] = $cache_duration;
        }

        if (isset($resp['format']['duration'])) {
            $current["playtimeSeconds"] = human_playtime($resp['format']['duration']);
        } else {
            $current["playtimeSeconds"] = "Unknown";
        }

    }
    array_push($list, $current);
}
