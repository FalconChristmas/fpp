<?
/**
 * Get origin commits
 *
 * Returns a list of commits present in the `origin` (GitHub) but not in the local repository.
 *
 * @route GET /api/git/originLog
 * @response 200 Commits in origin not yet in local
 * ```json
 * {
 *   "status": "OK",
 *   "rows": [
 *     {
 *       "hash": "95ccb370e45272d8aed76aabfa55e60d489a8280",
 *       "author": "GithubUser1",
 *       "msg": "Use our SaveJsonToString() when generating MQTT warnings JSON message."
 *     },
 *     {
 *       "hash": "2fad5ad941baea49edaab834429343b42981bcc5",
 *       "author": "GithubUser2",
 *       "msg": "Move Playlist initialization into main() via Player::Init()"
 *     }
 *   ]
 * }
 * ```
 */
function GetGitOriginLog()
{
    $rows = array();
    global $fppDir;

    exec("$fppDir/scripts/git_fetch", $log);
    unset($log);
    exec("$fppDir/scripts/git_origin_log", $log);

    foreach ($log as $line) {
        if (str_starts_with($line, "Git changes") || str_starts_with($line, "=========")) {
            continue;
        }
        $pos = strpos($line, "~~~~");
        $elements = explode("~~~~", $line);
        if ($pos > 0) {
            $h = $elements[0];
            $a = $elements[1];
            $msg = $elements[2];
            $row = array("hash" => $h, "author" => $a, "msg" => $msg);
            array_push($rows, $row);
        }
    }

    $rc = array("status" => "OK", "rows" => $rows);

    return json($rc);
}

/**
 * Discard local changes
 * Performs a hard reset on the current branch, discarding any local changes.
 *
 * @route POST /api/git/reset
 * @response 200 Reset complete
 * ```json
 * {
 *   "status": "OK",
 *   "log": [
 *     "HEAD is now at a1b65d43 Git Reset moved - #944",
 *     "Entering 'external/RF24'",
 *     "HEAD is now at ebc3abe Fix typo, missing space."
 *   ]
 * }
 * ```
 */
function GitReset()
{
    global $fppDir;
    exec("$fppDir/scripts/git_reset", $log);

    $rc = array("status" => "OK", "log" => $log);
    return json($rc);
}

/**
 * Get local repo status
 *
 * Returns the status of the local git branch, including any dirty files.
 *
 * @route GET /api/git/status
 * @response 200 Local repository status
 * ```json
 * {
 *   "status": "OK",
 *   "log": "On branch master\nYour branch is up to date with 'origin/master'."
 * }
 * ```
 */
function GitStatus()
{
    global $fppDir;

    $fullLog = "";
    exec("$fppDir/scripts/git_status", $log);
    $fullLog .= implode("\n", $log);

    $rc = array("status" => "OK", "log" => $fullLog);

    return json($rc);
}

/**
 * Checks whether a release asset filename matches the current device's platform and architecture.
 *
 * @param string $name     Asset filename to test (e.g. "Pi-7.0.fppos").
 * @param array  $settings Global FPP settings array providing OSImagePrefix and Is64Bit.
 * @return bool True if the filename matches this device; false otherwise.
 */
function matchesDeviceOSImage($name, $settings)
{
    if (!str_ends_with($name, ".fppos") || !isset($settings['OSImagePrefix']) || $settings['OSImagePrefix'] == "") {
        return false;
    }

    $prefix = $settings['OSImagePrefix'];
    $is64Bit = !empty($settings['Is64Bit']);

    if ($prefix == "Pi" || $prefix == "Pi64") {
        if (!(str_starts_with($name, "Pi-") || str_starts_with($name, "Pi64-"))) {
            return false;
        }
    } else if ($prefix == "BBB" || $prefix == "BB64") {
        if (!(str_starts_with($name, "BBB-") || str_starts_with($name, "BB64-"))) {
            return false;
        }
    } else if (!str_starts_with($name, $prefix . "-")) {
        return false;
    }

    // Match explicit architecture markers when they are present.
    $has64Marker = preg_match('/(^|[-_])(64|64bit|aarch64|arm64)([-_.]|$)/i', $name) === 1;
    $has32Marker = preg_match('/(^|[-_])(32|32bit|armv7|armv7l|armhf|arm32)([-_.]|$)/i', $name) === 1;

    if ($has64Marker && !$is64Bit) {
        return false;
    }
    if ($has32Marker && $is64Bit) {
        return false;
    }

    // Preserve legacy behavior if marker isn't present.
    if (!$has64Marker && !$has32Marker) {
        if ($is64Bit && ($prefix == "Pi64" || $prefix == "BB64")) {
            return str_starts_with($name, $prefix . "-");
        }
        if (!$is64Bit && ($prefix == "Pi" || $prefix == "BBB")) {
            return str_starts_with($name, $prefix . "-");
        }
    }

    return true;
}

/**
 * Get releases for OS
 *
 * Returns lists of `.fppos` files available locally or on GitHub for the current platform.
 * If the `{All}` path parameter is `"all"`, returns all releases regardless of platform.
 *
 * @route GET /api/git/releases/os/{All}
 * @response 200 Available OS release assets
 * ```json
 * {
 *   "status": "OK",
 *   "downloaded": ["Pi-v4.4.fppos", "Pi-5.0-alpha1.fppos"],
 *   "files": [
 *     {
 *       "tag": "5.1",
 *       "release_name": "5.1",
 *       "filename": "Pi-5.1.1.fppos",
 *       "url": "https://github.com/FalconChristmas/fpp/releases/download/5.1/Pi-5.1.1.fppos",
 *       "asset_id": 42917234,
 *       "downloaded": false,
 *       "size": 0,
 *       "prerelease": false
 *     }
 *   ]
 * }
 * ```
 */
function GitOSReleases()
{
    global $settings;
    global $uploadDirectory;
    $platforms = params('All');

    $existingFiles = getFileList($uploadDirectory, "fppos");

    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, "https://api.github.com/repos/FalconChristmas/fpp/releases?per_page=100");
    curl_setopt($curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.2; WOW64; rv:17.0) Gecko/20100101 Firefox/17.0");
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 4000);
    $request_content = curl_exec($curl);
    curl_close($curl);

    $rc = array('status' => 'ERROR');

    if ($request_content === false) {
        $rc['status'] = "Error";
        $rc['message'] = curl_error($curl);
    } else {
        $data = json_decode($request_content, true);
        $rc["downloaded"] = $existingFiles;
        $releases = array();
        foreach ($data as $r) {
            if (isset($r["assets"]) && $settings['OSImagePrefix'] != "") {
                foreach ($r["assets"] as $file) {
                    $name = $file["name"];
                    if (str_ends_with($name, ".fppos")) {
                        if ($platforms == "all") {
                            $row = array();
                            $row["tag"] = $r["tag_name"];
                            $row["release_name"] = $r["name"];
                            $row["filename"] = $name;
                            $row["url"] = $file["browser_download_url"];
                            $row["asset_id"] = $file["id"];
                            $row["downloaded"] = in_array($name, $existingFiles);
                            $row["size"] = $file["size"];
                            $row["prerelease"] = $r["prerelease"];
                            array_push($releases, $row);
                        } else if (matchesDeviceOSImage($name, $settings)) {
                            $row = array();
                            $row["tag"] = $r["tag_name"];
                            $row["release_name"] = $r["name"];
                            $row["filename"] = $name;
                            $row["url"] = $file["browser_download_url"];
                            $row["asset_id"] = $file["id"];
                            $row["downloaded"] = in_array($name, $existingFiles);
                            $row["size"] = $file["size"];
                            $row["prerelease"] = $r["prerelease"];
                            array_push($releases, $row);
                        }
                    }
                }
            }
        }
        $rc['status'] = 'OK';
        $rc['files'] = $releases;
    }

    return json($rc);
}

/**
 * Get release asset sizes
 *
 * Returns release asset size information from the GitHub `FalconChristmas/fpp` releases API.
 *
 * @route GET /api/git/releases/sizes
 * @response 200 Release asset sizes
 * ```json
 * [
 *   "BBB-nightly_2026-05.fppos,1161494528",
 *   "BBB-10.0-alpha_2026-02.fppos,884658176"
 * ]
 * ```
 */
function GitOSReleaseSizes()
{
    global $settings;

    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, "https://api.github.com/repos/FalconChristmas/fpp/releases?per_page=100");
    curl_setopt($curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.2; WOW64; rv:17.0) Gecko/20100101 Firefox/17.0");
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 4000);
    $request_content = curl_exec($curl);
    curl_close($curl);

    $rc = "";

    if ($request_content != false) {
        $data = json_decode($request_content, true);
        foreach ($data as $r) {
            if (isset($r["assets"]) && $settings['OSImagePrefix'] != "") {
                foreach ($r["assets"] as $file) {
                    $name = $file["name"];
                    if (matchesDeviceOSImage($name, $settings)) {
                        $rc = $rc . $name . "," . $file["size"] . "\n";
                    }
                }
            }
        }

    } else {
        $rc = "Error: " . curl_error($curl) . "\n";
    }

    return $rc;
}

/**
 * Get local branches
 *
 * Returns an array of branches available to switch to, filtering out obsolete version branches
 * and Dependabot branches.
 *
 * @route GET /api/git/branches
 * @response 200 Available local branches
 * ```json
 * ["master", "v7.3", "v7.2", "v7.1", "v7.0"]
 * ```
 */
function GitBranches()
{
    $rows = array();
    global $fppDir, $settings;

    // Get the remote parameter from the query string, default to 'origin'
    $remote = isset($_GET['remote']) ? $_GET['remote'] : (isset($settings['gitRemote']) ? $settings['gitRemote'] : 'origin');

    // Validate remote name to prevent injection
    if (!preg_match('/^[a-zA-Z0-9_-]+$/', $remote)) {
        $remote = 'origin';
    }

    exec("$fppDir/scripts/git_fetch", $log);
    unset($log);
    exec("git -C " . escapeshellarg($fppDir) . " branch -l --remotes", $log);

    foreach ($log as $line) {
        $line = trim($line);
        if (str_starts_with($line, "$remote/")) {
            $branch = substr($line, strlen($remote) + 1);

            if (
                !(preg_match("*v[01]\.[0-9x]*", $branch)   // very very old v0.x and v1.x branches
                    || preg_match("*v2\.[0-9x]*", $branch)   // old v2.x branchs, that can no longer work (wrong lib versions)
                    || preg_match("*v3\.[0-9x]*", $branch)   // old v3.x branchs, that can no longer work (wrong libhttp versions)
                    || preg_match("*v4\.[0-9x]*", $branch)   // old v4.x branchs, that can no longer work (wrong vlc versions)
                    || preg_match("*v5\.[0-9x]*", $branch)   // old v5.x branchs, that can no longer work (wrong OS versions)
                    || preg_match("*v6\.[0-9x]*", $branch)   // old v6.x branchs, below the FPP7 support floor
                    || str_starts_with($branch, "dependabot/")    // dependabot created branches
                    || str_starts_with($branch, "HEAD ")
                )
            ) {
                array_push($rows, $branch);
            }

        }
    }

    return json($rows);
}
?>