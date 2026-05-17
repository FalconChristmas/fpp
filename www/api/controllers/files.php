<?

/**
 * Maps a filename to its media directory based on file extension.
 * Falls back to plugin-registered extension mappings in `pluginInfo.json` if no built-in match is found.
 *
 * @param string $filename Filename to inspect.
 * @return string Absolute directory path, or empty string if no match found.
 */
function MapExtention($filename)
{
    global $mediaDirectory, $settings;

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
        $pluginDirectory = GetDirSetting("plugins");
        if (file_exists($pluginDirectory)) {
            foreach (scandir($pluginDirectory) as $plugin) {
                if (!in_array($plugin, array('.', '..'))) {
                    $pluginInfoFile = $pluginDirectory . "/" . $plugin . "/pluginInfo.json";
                    if (file_exists($pluginInfoFile)) {
                        $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
                        if (isset($pluginConfig["fileExtensions"])) {
                            foreach ($pluginConfig["fileExtensions"] as $key => $value) {
                                if (str_ends_with($filename, $key) && isset($value["folder"])) {
                                    return $mediaDirectory . "/" . $value["folder"];
                                }
                            }
                        }
                    }
                }
            }
        }
        return "";
    }
}

/**
 * Resolves a directory key name to its absolute path using settings, then
 * falls back to extension-based mapping.
 *
 * @param string $dirName Logical directory name (e.g. "music", "sequences").
 * @return string Absolute directory path, or empty string if not found.
 */
function MapDirectoryKey($dirName)
{
    $dir = GetDirSetting($dirName);
    if ($dir == "") {
        $dir = MapExtention($dirName);
    }
    return $dir;
}

/**
 * Copy file
 *
 * Copies the specified file from `:source` to `:dest` within the given directory.
 *
 * @route POST /api/file/{DirName}/copy/{source}/{dest}
 * @response 200 File copied successfully
 * ```json
 * {
 *   "status": "success",
 *   "original": "test.py",
 *   "new": "test2.py"
 * }
 * ```
 */
function files_copy()
{
    $filename = params("source");
    $newfilename = params("dest");
    $dir = params("DirName");

    $dir = MapDirectoryKey($dir);

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

/**
 * Rename file
 *
 * Renames the specified file from `:source` to `:dest` within the given directory.
 *
 * @route POST /api/file/{DirName}/rename/{source}/{dest}
 * @response 200 File renamed successfully
 * ```json
 * {
 *   "status": "success",
 *   "original": "test.py",
 *   "new": "test2.py"
 * }
 * ```
 */
function files_rename()
{
    $filename = params("source");
    $newfilename = params("dest");
    $dir = params("DirName");
    $dir = MapDirectoryKey($dir);

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

/**
 * Builds the file listing for a directory. Supports `?nameOnly=1` for a flat
 * name array, otherwise returns an array of file detail objects. For `music`
 * and `video` directories, playtime metadata is appended from `ffprobe`.
 *
 * @param string $dirName Absolute path to the directory to scan.
 * @param string $prefix  Optional prefix to prepend to each returned filename.
 * @return array Array of filename strings (nameOnly mode) or file detail arrays.
 */
function GetFilesHelper($dirName, $prefix = '')
{
    global $SUDO;

    if ($prefix != '') {
        $prefix .= '/';
    }

    $doSudo = $SUDO;
    $user = get_current_user();
    if ($user == "fpp" || $user == "root") {
        //don't need sudo to do the find so skip it to avoid the pam auth logs
        $doSudo = "";
    }

    // if ?nameOnly=1 was passed, then just array of names
    if (isset($_GET['nameOnly']) && ($_GET['nameOnly'] == '1')) {
        $rc = array();
        $filelist = array();
        exec("$doSudo find $dirName -type f -follow -printf \"%P\n\"", $filelist);
        foreach ($filelist as $fileName) {
            if ($fileName != '.' && $fileName != '..') {
                if (!preg_match("//u", $fileName)) {
                    $fileName = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $fileName);
                }
                array_push($rc, $prefix . $fileName);
            }
        }

        if (strtolower(params("DirName")) == "logs") {
            array_push($rc, "/var/log/messages");
            array_push($rc, "/var/log/syslog");
        }
        sort($rc);
        return $rc;
    } else {
        $files = array();
        $subDirList = array();
        exec("$doSudo find $dirName -type d -follow -printf \"%P|||%T@\n\" | sort", $subDirList);
        foreach ($subDirList as $dirDetails) {
            $Details = explode("|||", $dirDetails);
            $fileName = $Details[0];
            $mTime = $Details[1];
            if ($fileName != "") {
                $current = array();
                $current["name"] = $prefix . $fileName;
                $current["mtime"] = date('m/d/y  h:i A', $mTime);
                $current["sizeBytes"] = 0;
                $current["sizeHuman"] = 'Directory';

                $entries = array($current);

                $files = array_merge($files, $entries);
            }
        }

        $filelist = array();
        exec("$doSudo find $dirName -type f -follow -printf \"%P|||%s|||%T@\n\" | sort", $filelist);

        foreach ($filelist as $fileDetails) {
            $Details = explode("|||", $fileDetails);
            $fileName = $Details[0];
            $mTime = $Details[2];
            $Size = $Details[1];
            if (!preg_match("//u", $fileName)) {
                $fileName = iconv("ISO-8859-1", 'UTF-8//TRANSLIT', $fileName);
            }
            $current = array();
            $current["name"] = $prefix . $fileName;
            $current["mtime"] = date('m/d/y  h:i A', $mTime);
            $sizeInt = intval($Size);
            if (PHP_INT_SIZE === 4 && $sizeInt < PHP_INT_MAX) {
                $current["sizeBytes"] = $sizeInt;
            } else {
                $current["sizeBytes"] = $Size;
            }
            $current["sizeHuman"] = humanFileSize($Size);

            if (strpos(strtolower($dirName), "music") !== false || strpos(strtolower($dirName), "video") !== false) {

                //Check the cache first
                $cache_duration = media_duration_cache($fileName, null, $Size);
                //cache duration will be null if not in cache, then retrieve it
                if ($cache_duration == null) {

                    $resp = GetMetaDataFromFFProbe($fileName);

                    //cache it
                    if (isset($resp['format']['duration'])) {
                        media_duration_cache($fileName, $resp['format']['duration'], $Size);
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

            $entries = array($current);

            $files = array_merge($files, $entries);

        }

        if (strtolower(params("DirName")) == "logs") {
            if (file_exists("/var/log/messages")) {
                GetFileInfo($files, "", "/var/log/messages");
            }

            if (file_exists("/var/log/syslog")) {
                GetFileInfo($files, "", "/var/log/syslog");
            }

        }

        return $files;
    }
}

/**
 * Get all files
 *
 * Returns a list of files in the specified media directory.
 *
 * @route GET /api/files/{DirName}
 * @param bool nameOnly When `1`, return a flat array of filenames instead of the default object envelope
 * @response 200 Listing of files
 * ```json
 * {
 *   "status": "ok",
 *   "files": [
 *     {
 *       "name": "Christmas Every Day.mp3",
 *       "mtime": "09/23/20  07:47 PM",
 *       "sizeBytes": 7929000,
 *       "sizeHuman": "7.56MB",
 *       "playtimeSeconds": "03m:46s"
 *     }
 *   ]
 * }
 * ```
 */
function GetFiles()
{
    $dirName = params("DirName");
    check($dirName, "dirName", __FUNCTION__);
    $dirName = MapDirectoryKey($dirName);
    if ($dirName == "") {
        return json(array("status" => "Invalid Directory"));
    }

    if (isset($_GET['nameOnly']) && ($_GET['nameOnly'] == '1')) {
        return json(GetFilesHelper($dirName));
    } else {
        return json(array("status" => "ok", "files" => GetFilesHelper($dirName)));
    }
}

/**
 * Get plugin file info
 *
 * Returns plugin-specific file info for the specified file path. The plugin
 * name, extension category, and file path are read from route parameters.
 * The metadata command is defined in the plugin's `pluginInfo.json`.
 *
 * @route GET /api/file/info/{plugin}/{ext}/**
 * @response 200 Plugin-specific file information
 * ```json
 * {}
 * ```
 */
function GetPluginFileInfo()
{
    global $mediaDirectory;

    $pluginName = params("plugin");
    $ext = params("ext");
    $fileName = params(0);

    $pluginDirectory = GetDirSetting("plugins") . "/" . $pluginName;
    if (is_dir($pluginDirectory)) {
        $pluginInfoFile = $pluginDirectory . "/pluginInfo.json";
        if (file_exists($pluginInfoFile)) {
            $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
            if (
                isset($pluginConfig["fileExtensions"])
                && isset($pluginConfig["fileExtensions"][$ext])
                && isset($pluginConfig["fileExtensions"][$ext]["infoCommand"])
            ) {
                $cmd = $pluginDirectory . "/" . $pluginConfig["fileExtensions"][$ext]["infoCommand"];
                $cmd .= " ";

                if (isset($pluginConfig["fileExtensions"][$ext]["folder"])) {
                    $fileName = $mediaDirectory . "/" . $pluginConfig["fileExtensions"][$ext]["folder"] . "/" . $fileName;
                }
                $cmd .= escapeshellarg($fileName);
                send_header('Content-Type: application/json; charset=' . strtolower(option('encoding')));
                return shell_exec($cmd);
            }
        }
    }
    return json(array("status" => "ERROR: Could not find information for file $fileName"));
}

/**
 * Notifies the first matching plugin that a file has been uploaded by running
 * that plugin's registered `onUpload` handler command.
 *
 * @param string $dir      Absolute directory path where the file was placed.
 * @param string $filename Filename that was uploaded.
 * @return string Output from the plugin handler, or an error string if no handler found.
 */
function CallPluginFileUploaded($dir, $filename)
{
    $pluginDirectory = GetDirSetting("plugins");
    if (file_exists($pluginDirectory)) {
        foreach (scandir($pluginDirectory) as $plugin) {
            if ($plugin == "." || $plugin == "..") {
                continue;
            }
            $pluginInfoFile = $pluginDirectory . "/" . $plugin . "/pluginInfo.json";
            if (file_exists($pluginInfoFile)) {
                $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
                if (isset($pluginConfig["fileExtensions"])) {
                    foreach ($pluginConfig["fileExtensions"] as $key => $value) {
                        if (str_ends_with($filename, $key) && isset($value["onUpload"])) {
                            $cmd = $pluginDirectory . "/" . $plugin . "/" . $pluginConfig["fileExtensions"][$key]["onUpload"];
                            $cmd .= " ";
                            $cmd .= escapeshellarg($dir . "/" . $filename);
                            $ret = shell_exec($cmd);
                            clearstatcache();
                            return $ret;
                        }
                    }
                }
            }
        }
    }
    return "Could not find plugin to handle " . $filename;
}

/**
 * Notify plugin of upload
 *
 * Notifies any plugin that has registered an `onUpload` handler for the given
 * file extension. `:ext` is the extension category and `**` is the file path.
 *
 * @route GET /api/file/onUpload/{ext}/**
 * @response 200 Plugin notified of upload
 * ```json
 * {"status": "OK"}
 * ```
 */
function PluginFileOnUpload()
{
    global $mediaDirectory;

    $ext = params("ext");
    $fileName = params(0);
    return CallPluginFileUploaded($mediaDirectory . "/" . $ext, $fileName);
}

/**
 * Moves a newly uploaded file from the upload staging directory to the correct
 * media subdirectory as registered by a plugin's `pluginInfo.json`, then fires
 * the plugin's `onUpload` handler.
 *
 * @param string $uploadDir Absolute path to the staging upload directory.
 * @param string $filename  Filename to move.
 * @return bool True on success, false if the rename failed.
 */
function MovePluginFile($uploadDir, $filename)
{
    global $mediaDirectory;
    $pluginDirectory = GetDirSetting("plugins");
    if (file_exists($pluginDirectory)) {
        foreach (scandir($pluginDirectory) as $plugin) {
            if (!in_array($plugin, array('.', '..'))) {
                $pluginInfoFile = $pluginDirectory . "/" . $plugin . "/pluginInfo.json";
                if (file_exists($pluginInfoFile)) {
                    $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
                    if (isset($pluginConfig["fileExtensions"])) {
                        foreach ($pluginConfig["fileExtensions"] as $key => $value) {
                            if (str_ends_with($filename, $key) && isset($value["folder"])) {
                                if (!rename($uploadDir . "/" . $filename, $mediaDirectory . "/" . $value["folder"] . "/" . $filename)) {
                                    return false;
                                }
                                CallPluginFileUploaded($mediaDirectory . "/" . $value["folder"], $filename);
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

/**
 * Get file contents
 *
 * Downloads the specified file from a media directory.
 *
 * @route GET /api/file/{DirName}/**
 * @param int tail Return the last N lines instead of the whole file
 * @param bool play When `1`, set a playback-oriented content type instead of a forced attachment
 * @param bool attach When `1`, force attachment download for images
 * @response 200 File contents; content type varies by file extension and query params
 * ```bytes
 * [Content-Type: application/octet-stream]
 * ```
 * @response 404 File not found
 * ```text
 * File <path> does not exist.
 * ```
 */
function GetFile()
{
    $dirName = params("DirName");
    $fileName = params(0);
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

/**
 * Core implementation for file download. Resolves the logical directory name,
 * sets appropriate `Content-Type` and `Content-Disposition` headers based on mode,
 * and streams the file (or its tail) to the response.
 *
 * @param string $dir      Logical directory name (e.g. "Images", "Logs").
 * @param string $filename Relative filename within the directory.
 * @param int    $lines    Number of tail lines to return; -1 for the full file.
 * @param int    $play     1 to set a playback content type instead of download.
 * @param int    $attach   1 to force attachment Content-Disposition for images.
 * @return void
 */
function GetFileImpl($dir, $filename, $lines, $play, $attach)
{
    $isImage = 0;
    $isLog = 0;
    if ($dir == 'Images') {
        $isImage = 1;
    } else if ($dir == "Logs") {
        $isLog = 1;
    }

    $dir = MapDirectoryKey($dir);

    if ($dir == "") {
        return;
    }

    if ($isLog == 1 && (substr($filename, 0, 8) == "var/log/")) {
        $dir = "/var/log";
        $filename = substr($filename, 8);
    }

    if (!file_exists($dir . '/' . $filename) || !is_file($dir . '/' . $filename)) {
        http_response_code(404);
        echo "File $dir/$filename does not exist.";
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

/**
 * Locates a file within a directory, trying the raw name, URL-decoded name,
 * sanitized name, and ISO-8859-1 to UTF-8 conversion in order.
 *
 * @param string $dir      Absolute directory path to search in.
 * @param string $filename Filename to locate.
 * @return string The resolved filename (possibly transformed), or the original if not found.
 */
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

/**
 * Move file
 *
 * Moves the specified file from the `uploads` directory to the correct media
 * subfolder based on its extension, returning a status of `OK` or an error
 * message if not successful.
 *
 * @route GET /api/file/move/{fileName}
 * @response 200 File moved to media directory
 * ```json
 * {"status": "OK"}
 * ```
 */
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
        } else {
            if (!MovePluginFile($uploadDirectory, $file)) {
                $status = "ERROR: Couldn't move file";
                return json(array("status" => $status));
            }
        }
    } else {
        $status = "ERROR: Couldn't find file '" . $file . "' in upload directory";
    }
    return json(array("status" => $status));
}

/**
 * Get zip file of directories
 *
 * Downloads all files in the specified directory (or comma-separated list of
 * directories) as a zip archive. `logs` and `config` are handled specially to
 * include system log and config files.
 *
 * @route GET /api/files/zip/{DirNames}
 * @response 200 Binary file stream of the compressed system archive.
 * ```bytes
 * [Raw Binary Stream: application/zip]
 * Content-Disposition: attachment; filename="FPP_backup_20230124.zip"
 * Size: Variable (compressed zip archive)
 * ```
 */
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
            $dir = MapDirectoryKey($dirName);
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

/**
 * Adds log files and configuration files to the given `ZipArchive`. Includes
 * system logs, FPP config files, ALSA configuration, and `git` status/diff.
 *
 * @param ZipArchive $zip The archive to add files to.
 * @return void
 */
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

/**
 * Adds FPP configuration files (channel outputs, schedules, settings, etc.)
 * to the given `ZipArchive`, scrubbing sensitive values from `JSON` files.
 *
 * @param ZipArchive $zip The archive to add files to.
 * @return void
 */
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
        "config/ci-dmx.json",
        "config/co-bbbStrings.json",
        "config/co-pwm.json",
        "config/commandPresets.json",
        "config/co-other.json",
        "config/co-pixelStrings.json",
        "config/co-universes.json",
        "config/instantCommand.json",
        "config/model-overlays.json",
        "config/outputprocessors.json",
        "config/schedule.json",
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

/**
 * Adds all files in a directory to the given `ZipArchive` under the specified
 * archive name prefix.
 *
 * @param ZipArchive $zip       The archive to add files to.
 * @param string     $name      Archive path prefix for the files.
 * @param string     $directory Absolute path to the directory to add.
 * @return void
 */
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

/**
 * Recursively removes a directory and all of its contents.
 *
 * @param string $dir Absolute path to the directory to remove.
 * @return void
 */
function removeDir(string $dir): void
{
    $it = new RecursiveDirectoryIterator($dir, RecursiveDirectoryIterator::SKIP_DOTS);
    $files = new RecursiveIteratorIterator(
        $it,
        RecursiveIteratorIterator::CHILD_FIRST
    );
    foreach ($files as $file) {
        if ($file->isDir()) {
            rmdir($file->getPathname());
        } else {
            unlink($file->getPathname());
        }
    }
    rmdir($dir);
}

/**
 * Delete file or directory
 *
 * Deletes the specified file or directory from a media directory. Validates
 * the resolved path against the allowed base directory to prevent path traversal.
 *
 * @route DELETE /api/file/{DirName}/**
 * @response 200 File or directory deleted
 * ```json
 * {
 *   "status": "OK",
 *   "file": "block_driveways.xbkp",
 *   "dir": "uploads"
 * }
 * ```
 */
function DeleteFile()
{
    global $uploadDirectory;

    $status = "File not found";
    $dirName = params("DirName");
    $dir = MapDirectoryKey($dirName);
    $fileName = params(0);

    if ($fileName == "" && $dir == $uploadDirectory) {
        // Hitting cancel on an upload will call delete with no filename
        return json(array("status" => "Invalid parameters", "file" => $fileName, "dir" => $dirName));
    }

    // Avoid too much saniziation so that we can delete files with unicode in them
    $allowedDir = realpath($dir); // Allowed base directory
    $constructedPath = "$dir/$fileName";
    $fullPath = realpath($constructedPath); // Full resolved path of the target file

    if (!$allowedDir || !$fullPath || strpos($fullPath, $allowedDir) !== 0) {
        $status = "Invalid path: directory traversal detected or file outside allowed directory";
    } else if (!file_exists($fullPath)) {
        $status = "File Not Found";
    } else if (is_dir($fullPath)) {
        removeDir($fullPath);
        $status = "OK";
    } else {
        if (unlink($fullPath)) {
            $status = "OK";
        } else {
            $errorDetails = error_get_last();
            $status = "Unable to delete file: " . ($errorDetails['message'] ?? 'Unknown');
        }
    }

    return json(array("status" => $status, "file" => $fileName, "dir" => $dirName));
}

/**
 * Emulates `fseek()` for positions beyond `PHP_INT_MAX` on 32-bit systems by
 * seeking to `PHP_INT_MAX` and then advancing in 8 KB chunks.
 *
 * @param resource $fp  File handle to seek.
 * @param string   $pos BC math string representing the byte offset to seek to.
 * @return void
 */
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

/**
 * Upload file
 *
 * Handles chunked file uploads via `PATCH` (TUS-style). A `POST` to the same route
 * initiates the session and returns a unique upload ID. Each `PATCH` request
 * delivers a chunk identified by `Upload-Name`, `Upload-Offset`, and `Upload-Length`
 * headers; when all chunks arrive, the file is assembled.
 *
 * @route POST /api/file/{DirName}
 * @route PATCH /api/file/{DirName}
 * @response 200 Upload chunk received
 * ```json
 * {
 *   "status": "OK",
 *   "file": "block_driveways.xbkp",
 *   "dir": "uploads",
 *   "size": 1048576
 * }
 * ```
 */
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

    $dir = MapDirectoryKey($dirName);
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
                if (file_exists($fullPath)) {
                    $file_handle = fopen($fullPath, 'r+b');
                    $wouldBlock = false;
                    if (flock($file_handle, LOCK_EX | LOCK_NB, $wouldBlock)) {
                        ftruncate($file_handle, 0);
                    } else {
                        fclose($file_handle);
                        if (!str_ends_with($fullPath, ".fseq")) {
                            http_response_code(500);
                            return json(array("status" => "failed", "file" => $fileName, "dir" => $dirName, "size" => $size, "error" => "Could not lock file for writing"));
                        }
                        $file_handle = fopen($fullPath . '.replace', 'w+b');
                        flock($file_handle, LOCK_EX);
                    }
                } else {
                    error_log("Creating new file $fullPath");
                    // create empty file
                    $file_handle = fopen($fullPath, 'w+b');
                    flock($file_handle, LOCK_EX);
                }
            }
            $patch_handle = fopen($fullPath . '.patch.' . $offset, 'rb');
            while (!feof($patch_handle)) {
                $read = fread($patch_handle, 128 * 1024);
                fwrite($file_handle, $read);
            }
            fclose($patch_handle);
            unlink($fullPath . '.patch.' . $offset);
        }

        // done with file
        flock($file_handle, LOCK_UN);
        fclose($file_handle);

        if ($dirName != "upload" && $dirName != "uploads") {
            // uploads to the upload directory will have this called during MoveFile
            CallPluginFileUploaded($dir, $fileName);
        }
    }
    return json(array("status" => $status, "file" => $fileName, "dir" => $dirName, "size" => $size));
}

/**
 * Upload file to directory
 *
 * Uploads a file to the specified media directory.
 *
 * @route POST /api/file/{DirName}/{Name}
 * @param int bs Block size used for fragmented uploads
 * @param int sb Starting block index used for fragmented uploads
 * @response 200 File uploaded successfully
 * ```json
 * {
 *   "status": "OK",
 *   "file": "beepbeep.fseq",
 *   "dir": "sequences"
 * }
 * ```
 */
function PostFile()
{
    $status = "OK";
    $dirName = params("DirName");
    $fileName = params("Name");

    $dir = MapDirectoryKey($dirName);
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

/**
 * Appends a file detail entry to a list array. Includes name, mtime, size,
 * and (for `music`/`video` directories) playtime metadata from the `ffprobe` cache.
 *
 * @param array  &$list    The list to append the file entry to.
 * @param string $dirName  Absolute path to the directory containing the file.
 * @param string $fileName Relative filename within the directory.
 * @param string $prefix   Optional prefix to prepend to the returned filename.
 * @return void
 */
function GetFileInfo(&$list, $dirName, $fileName, $prefix = '')
{
    $fileFullName = $dirName . '/' . $fileName;
    $filesize = real_filesize($fileFullName);
    if (intval($filesize) < PHP_INT_MAX) {
        $filesize = intval($filesize);
    }
    $current = array();
    $current["name"] = $prefix . $fileName;
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

/**
 * Create subdirectory
 *
 * Creates a subdirectory inside the specified media directory.
 *
 * @route POST /api/dir/{DirName}/{SubDir}
 * @response 200 Subdirectory created
 * ```json
 * {
 *   "status": "OK",
 *   "subdir": "mySubDir",
 *   "dir": "sequences"
 * }
 * ```
 */
function CreateDir()
{
    $status = "Unable to create directory";
    $dirName = params("DirName");
    $subDir = params("SubDir");
    $sanitized = sanitizeFilename($subDir);
    $dir = MapDirectoryKey($dirName);
    $fullPath = "$dir/$subDir";

    if ($sanitized != $subDir) {
        $status = "Invalid subdirectory name";
    } else if ($dir == "") {
        $status = "Invalid Directory";
    } else if (file_exists($fullPath)) {
        $status = "Subdirectory already exists";
    } else {
        if (mkdir($fullPath)) {
            chmod($fullPath, 0755);
            $status = "OK";
        } else {
            $status = "Unable to delete file";
        }
    }

    return json(array("status" => $status, "subdir" => $subDir, "dir" => $dirName));
}

/**
 * Delete empty subdirectory
 *
 * Deletes an empty subdirectory from the specified media directory.
 *
 * @route DELETE /api/dir/{DirName}/{SubDir}
 * @response 200 Subdirectory deleted
 * ```json
 * {
 *   "status": "OK",
 *   "subdir": "mySubDir",
 *   "dir": "sequences"
 * }
 * ```
 */
function DeleteDir()
{
    $status = "SubDir not found";
    $dirName = params("DirName");
    $subDir = params("SubDir");
    $sanitized = sanitizeFilename($subDir);
    $dir = MapDirectoryKey($dirName);
    $fullPath = "$dir/$subDir";

    if ($sanitized != $subDir) {
        $status = "Invalid subdirectory name";
    } else if ($dir == "") {
        $status = "Invalid Directory";
    } else if (!file_exists($fullPath)) {
        $status = "Subdirectory '$subDir' does not exist";
    } else {
        if (rmdir($fullPath)) {
            $status = "OK";
        } else {
            $status = "Unable to delete subdirectory, does it contain any files?";
        }
    }

    return json(array("status" => $status, "subdir" => $subDir, "dir" => $dirName));
}

/**
 * Stream tail of file
 *
 * Streams the tail of a log file using Server-Sent Events (SSE). Only works
 * for files in the `logs` directory.
 *
 * @route GET /api/file/{DirName}/tailfollow/*
 * @param int lines Number of existing lines to seed into the stream, from 1 to 500, default 50
 * @response 200 Success
 * ```text
 * [Content-Type: text/event-stream]
 * [12-May-2026 01:24:10] NOTICE: fpm is running, pid 80
 * [12-May-2026 01:24:10] NOTICE: ready to handle connections
 * ```
 * @response 403 Forbidden directory
 * ```text
 * Tail follow is only allowed for log files.
 * ```
 * @response 403 Forbidden filename
 * ```text
 * Invalid file path.
 * ```
 * @response 404 File not found
 * ```text
 * File not found: <filename>
 * ```
 */
function TailFollowFile()
{
    $dirName = params("DirName");
    $fileName = params(0);

    // Only allow for Logs directory
    if (strtolower($dirName) != "logs") {
        http_response_code(403);
        echo "Tail follow is only allowed for log files.";
        return;
    }

    $dir = MapDirectoryKey($dirName);

    if ($dir == "") {
        http_response_code(403);
        echo "Invalid directory.";
        return;
    }

    if (substr($fileName, 0, 8) == "var/log/") {
        $dir = "/var/log";
        $fileName = substr($fileName, 8);
    }

    $fullPath = $dir . '/' . $fileName;

    // Validate path to prevent directory traversal
    $realDir = realpath($dir);
    $realPath = realpath($fullPath);

    if (!$realDir || !$realPath || strpos($realPath, $realDir) !== 0) {
        http_response_code(403);
        echo "Invalid file path.";
        return;
    }

    if (!is_file($realPath)) {
        http_response_code(404);
        echo "File not found: $fileName";
        return;
    }

    $lines = 50;
    if (isset($_GET['lines'])) {
        $lines = intval($_GET['lines']);
        if ($lines < 1)
            $lines = 1;
        if ($lines > 500)
            $lines = 500;
    }

    // Set up SSE headers BEFORE disabling output buffering. DisableOutputBuffering()
    // ends with flush(), which commits the HTTP response headers to the client. If the
    // Content-Type isn't set to text/event-stream before that flush, it stays the
    // default text/html and the browser's EventSource rejects the stream immediately
    // ("Connection error or stream ended").
    header('Content-Type: text/event-stream');
    header('Cache-Control: no-cache');
    header('Connection: keep-alive');
    header('X-Accel-Buffering: no');
    DisableOutputBuffering();

    // Open tail process with timeout wrapper to prevent orphaned processes
    $descriptorspec = array(
        0 => array("pipe", "r"),
        1 => array("pipe", "w"),
        2 => array("file", "/dev/null", "w")
    );

    // Wrap tail in timeout to kill process after 5 minutes of inactivity
    $cmd = "timeout 300 tail -n " . $lines . " -f " . escapeshellarg($realPath) . " 2>&1";
    $process = proc_open($cmd, $descriptorspec, $pipes);

    if (!is_resource($process)) {
        echo "event: error\n";
        echo "data: Failed to start tail process\n\n";
        flush();
        return;
    }

    fclose($pipes[0]);
    stream_set_blocking($pipes[1], false);

    $buffer = '';
    $lastSend = microtime(true);
    $startTime = microtime(true);
    $lastActivity = microtime(true);
    $heartbeatInterval = 30; // Send heartbeat every 30 seconds

    while (!feof($pipes[1])) {
        $read = array($pipes[1]);
        $write = $except = null;

        if (stream_select($read, $write, $except, 0, 100000)) {
            $data = fread($pipes[1], 8192);
            if ($data !== false && strlen($data) > 0) {
                $buffer .= $data;
                $lastActivity = microtime(true);
            }
        }

        $now = microtime(true);

        // Send buffered data every 100ms or if buffer is large
        if ((strlen($buffer) > 0 && ($now - $lastSend) > 0.1) || strlen($buffer) > 4096) {
            // SSE format: each line must be prefixed with "data: "
            $lines = explode("\n", $buffer);
            foreach ($lines as $line) {
                echo "data: " . $line . "\n";
            }
            echo "\n";
            flush();
            $buffer = '';
            $lastSend = $now;
        }

        // Send periodic heartbeat to keep connection alive and detect disconnects
        if (($now - $lastSend) > $heartbeatInterval) {
            echo ": heartbeat\n\n";
            flush();
            $lastSend = $now;
        }

        // Check connection status more aggressively
        if (connection_aborted()) {
            break;
        }

        // Additional check: try to detect broken connection by checking output buffer
        if (@ob_get_level() > 0) {
            @ob_flush();
        }
        @flush();
    }

    // Cleanup
    fclose($pipes[1]);
    proc_terminate($process);
    proc_close($process);
}
