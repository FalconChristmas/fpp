<?
/**
 * Get origin commits
 *
 * Returns a list of commits present in the `origin` (GitHub) but not in the local repository.
 *
 * @route-v1 GET /git/originLog
 * @route-v2 GET /git/originLog
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
 * @route-v1 GET /git/reset
 * @route-v2 POST /git/reset
 * @badge-v1 "DEPRECATED" warning
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
 * @route-v1 GET /git/status
 * @route-v2 GET /git/status
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
 * Get releases for OS
 *
 * Returns lists of `.fppos` files available locally or on GitHub for the current platform.
 * If the `{All}` path parameter is `"all"`, returns all releases regardless of platform.
 *
 * @route-v1 GET /git/releases/os/{All}
 * @route-v2 GET /git/releases/os/{All}
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
    $request_error = curl_error($curl);
    curl_close($curl);

    $rc = array('status' => 'ERROR');
    $releases = array();
    $githubOk = false;

    if ($request_content !== false) {
        $githubOk = true;
        $data = json_decode($request_content, true);
        foreach ($data as $r) {
            if (isset($r["assets"]) && $settings['OSImagePrefix'] != "") {
                foreach ($r["assets"] as $file) {
                    $name = $file["name"];
                    if (str_ends_with($name, ".fppos")) {
                        if ($platforms == "all" || MatchesDeviceOSImage($name, $settings)) {
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
    }

    // When an "Upgrade Source" other than github.com is configured (another FPP
    // instance on the LAN), also list the .fppos images that FPP actually has in
    // its uploads dir, so images can be pulled from the LAN instead of GitHub --
    // including images GitHub has since rolled past (e.g. an older nightly). This
    // is done in addition to the GitHub list; entries present in both are tagged
    // with their source and pointed at the mirror.
    $upgradeSource = isset($settings['UpgradeSource']) ? $settings['UpgradeSource'] : 'github.com';
    if ($upgradeSource != '' && $upgradeSource != 'github.com') {
        $srcCurl = curl_init();
        curl_setopt($srcCurl, CURLOPT_URL, "http://" . fppUrlHost($upgradeSource) . "/api/files/uploads");
        curl_setopt($srcCurl, CURLOPT_FAILONERROR, true);
        curl_setopt($srcCurl, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($srcCurl, CURLOPT_CONNECTTIMEOUT_MS, 4000);
        curl_setopt($srcCurl, CURLOPT_TIMEOUT, 8);
        $srcContent = curl_exec($srcCurl);
        curl_close($srcCurl);
        $srcData = ($srcContent !== false) ? json_decode($srcContent, true) : null;
        if (isset($srcData["files"]) && is_array($srcData["files"])) {
            // Index the GitHub entries by filename so a source copy annotates the
            // existing entry rather than adding a duplicate row.
            $indexByName = array();
            foreach ($releases as $i => $rel) {
                $indexByName[$rel["filename"]] = $i;
            }
            foreach ($srcData["files"] as $sf) {
                $name = isset($sf["name"]) ? $sf["name"] : "";
                if (!str_ends_with($name, ".fppos")) {
                    continue;
                }
                if ($platforms != "all" && !MatchesDeviceOSImage($name, $settings)) {
                    continue;
                }
                $srcUrl = "http://" . fppUrlHost($upgradeSource) . "/api/file/uploads/" . $name;
                if (isset($indexByName[$name])) {
                    // Available from the source too -- pull it from the LAN and
                    // flag the origin so the UI can label it.
                    $releases[$indexByName[$name]]["source"] = $upgradeSource;
                    $releases[$indexByName[$name]]["url"] = $srcUrl;
                } else {
                    // Source-only image (GitHub no longer offers it).
                    $lname = strtolower($name);
                    $row = array();
                    $row["tag"] = "";
                    $row["release_name"] = "From " . $upgradeSource;
                    $row["filename"] = $name;
                    $row["url"] = $srcUrl;
                    // Synthetic, positive-integer asset id so about.php's numeric
                    // option-cleanup (parseInt(value) > 0) still clears it on reload.
                    $row["asset_id"] = sprintf("%u", crc32($name));
                    $row["downloaded"] = in_array($name, $existingFiles);
                    $row["size"] = isset($sf["sizeBytes"]) ? intval($sf["sizeBytes"]) : 0;
                    $row["prerelease"] = (strpos($lname, "nightly") !== false || strpos($lname, "alpha") !== false || strpos($lname, "beta") !== false);
                    $row["source"] = $upgradeSource;
                    array_push($releases, $row);
                    $indexByName[$name] = count($releases) - 1;
                }
            }
        }
    }

    if ($githubOk || !empty($releases)) {
        $rc['status'] = 'OK';
        $rc['downloaded'] = $existingFiles;
        $rc['files'] = $releases;
    } else {
        $rc['status'] = "Error";
        $rc['message'] = $request_error;
    }

    return json($rc);
}

/**
 * Get OS release notes for a tag
 *
 * Proxies the GitHub `FalconChristmas/fpp` release-by-tag API so the browser does
 * not call api.github.com directly (same pattern/UA/timeout as GitOSReleases).
 * Returns the raw GitHub release object on success; a non-200 status otherwise so
 * the caller's error handler fires.
 *
 * @route GET /api/git/releases/notes/{tag}
 * @response 200 GitHub release object for the tag
 * @response 400 Invalid tag
 * @response 404 Release not found for the tag
 */
function GitOSReleaseNotes()
{
    $tag = params('tag');

    // Release tags are simple (e.g. "9.5", "10.0-beta2"); reject anything else
    // before it reaches the GitHub URL.
    if (!preg_match('/^[A-Za-z0-9._-]+$/', $tag)) {
        http_response_code(400);
        return json(array('status' => 'ERROR', 'message' => 'Invalid tag'));
    }

    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, "https://api.github.com/repos/FalconChristmas/fpp/releases/tags/" . rawurlencode($tag));
    curl_setopt($curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.2; WOW64; rv:17.0) Gecko/20100101 Firefox/17.0");
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 4000);
    $request_content = curl_exec($curl);
    $http_code = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    curl_close($curl);

    if ($request_content === false || $http_code >= 400) {
        http_response_code(404);
        return json(array('status' => 'ERROR', 'message' => 'Release notes not found'));
    }

    $data = json_decode($request_content, true);
    if ($data === null) {
        http_response_code(502);
        return json(array('status' => 'ERROR', 'message' => 'Invalid response from GitHub'));
    }

    return json($data);
}

/**
 * Get release asset sizes
 *
 * Returns release asset size information from the GitHub `FalconChristmas/fpp` releases API.
 *
 * @route-v1 GET /git/releases/sizes
 * @route-v2 GET /git/releases/sizes
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
 * @route-v1 GET /git/branches
 * @route-v2 GET /git/branches
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