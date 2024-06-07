<?
// GET /api/git/originLog
function GetGitOriginLog()
{
    $rows = array();
    global $fppDir;

    exec("$fppDir/scripts/git_fetch", $log);
    unset($log);
    exec("$fppDir/scripts/git_origin_log", $log);

    foreach ($log as $line) {
        if (startsWith($line, "Git changes") || startsWith($line, "=========")) {
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

function GitReset()
{
    global $fppDir;
    exec("$fppDir/scripts/git_reset", $log);

    $rc = array("status" => "OK", "log" => $log);
    return json($rc);
}

// GET /api/git/status
function GitStatus()
{
    global $fppDir;

    $fullLog = "";
    exec("$fppDir/scripts/git_status", $log);
    $fullLog .= implode("\n", $log);

    $rc = array("status" => "OK", "log" => $fullLog);

    return json($rc);
}

// GET http://fpp/api/git/releases/os
// Returns fppos files for download
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
                    if (endsWith($name, ".fppos")) {
                        if ($platforms == "all") {
                            $row = array();
                            $row["tag"] = $r["tag_name"];
                            $row["release_name"] = $r["name"];
                            $row["filename"] = $name;
                            $row["url"] = $file["browser_download_url"];
                            $row["asset_id"] = $file["id"];
                            $row["downloaded"] = in_array($name, $existingFiles);
                            $row["size"] = $file["size"];
                            array_push($releases, $row);
                        } else if (startsWith($name, $settings['OSImagePrefix'])) {
                            $row = array();
                            $row["tag"] = $r["tag_name"];
                            $row["release_name"] = $r["name"];
                            $row["filename"] = $name;
                            $row["url"] = $file["browser_download_url"];
                            $row["asset_id"] = $file["id"];
                            $row["downloaded"] = in_array($name, $existingFiles);
                            $row["size"] = $file["size"];
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

// GET http://fpp/api/git/releases/sizes
// Returns fppos file sizes
// Should re-write this to call GetOSReleases() and parse instead of hitting github directly.
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
                    if (endsWith($name, ".fppos") && startsWith($name, $settings['OSImagePrefix'])) {
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

// GET http://fpp/api/git/branches
// Returns array of supported branch names
function GitBranches()
{
    $rows = array();
    global $fppDir;

    exec("$fppDir/scripts/git_fetch", $log);
    unset($log);
    exec("git -C " . escapeshellarg($fppDir) . " branch -l --remotes", $log);

    foreach ($log as $line) {
        $line = trim($line);
        if (startsWith($line, "origin/")) {
            $branch = substr($line, 7);

            if (
                !(preg_match("*v[01]\.[0-9x]*", $branch)   // very very old v0.x and v1.x branches
                    || preg_match("*v2\.[0-9x]*", $branch)   // old v2.x branchs, that can no longer work (wrong lib versions)
                    || preg_match("*v3\.[0-9x]*", $branch)   // old v3.x branchs, that can no longer work (wrong libhttp versions)
                    || preg_match("*v4\.[0-9x]*", $branch)   // old v4.x branchs, that can no longer work (wrong vlc versions)
                    || preg_match("*v4\.[0-9x]*", $branch)   // old v4.x branchs, that can no longer work (wrong vlc versions)
                    || preg_match("*v5\.[0-9x]*", $branch)   // old v5.x branchs, that can no longer work (wrong OS versions)
                    || startsWith($branch, "dependabot/")    // dependabot created branches
                    || startsWith($branch, "HEAD ")
                )
            ) {
                array_push($rows, $branch);
            }

        }
    }

    return json($rows);
}
?>