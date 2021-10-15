
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
        $pos = strpos($line, " ");
        if ($pos > 0) {
            $h = substr($line, 0, $pos);
            $row = array("hash" => $h, "msg" => substr($line, $pos + 1));
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
                    if (endsWith($name, ".fppos") && startsWith($name, $settings['OSImagePrefix'])) {
                        $row = array();
                        $row["tag"] = $r["tag_name"];
                        $row["release_name"] = $r["name"];
                        $row["filename"] = $name;
                        $row["url"] = $file["browser_download_url"];
                        $row["asset_id"] = $file["id"];
                        $row["downloaded"] = in_array($name, $existingFiles);
                        array_push($releases, $row);
                    }
                }
            }
        }
        $rc['status'] = 'OK';
        $rc['files'] = $releases;
    }

    return json($rc);
}

?>