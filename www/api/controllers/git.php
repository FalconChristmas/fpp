
<?
// GET /api/git/originLog
function GetGitOriginLog() {
    $rows = Array();
    global $fppDir;

    exec("$fppDir/scripts/git_fetch", $log);
    unset($log);
    exec("$fppDir/scripts/git_origin_log", $log);
    
    foreach($log as $line) {
        if (startsWith($line, "Git changes") || startsWith($line, "=========")) {
            continue;
        }
        $pos = strpos($line, " ");
        if ($pos > 0) {
            $h = substr($line, 0, $pos);
            $row = array("hash" => $h, "msg" => substr($line,$pos+1));
            array_push($rows, $row);
        }
    }

    $rc = Array("status" => "OK", "rows" => $rows);

    return json($rc);
}

?>