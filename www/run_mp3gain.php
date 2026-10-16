<?

$skipJSsettings = 1;
require_once("common.php");
require_once("common/oplog.inc.php");

DisableOutputBuffering();
// Finish the job if the browser goes away: a closed tab or dropped connection
// used to abort PHP part way through, leaving the command it ran half done.
ignore_user_abort(true);

print("Running MP3Gain\n\n");

$files = json_decode(file_get_contents('php://input'));
$params = "mp3gain -r -k ";
$dir = GetDirSetting("music");
foreach ($files as $f) {
    $params = $params . " " . escapeshellarg($dir . "/" . $f);
}

// Was: a private mp3gain.log, written with a "====" banner + date and a tee.
// Nothing ever read that file, and one more log to know about is one more place
// to look; mp3gain output now joins the fppd.log timeline like every other
// operation. popen rather than system() so the browser still gets the output
// live (DisableOutputBuffering above) while each line is also logged --
// system() would stream but leave nothing to log.
FppdLogLine('run_mp3gain.php', 'MediaOut', 'MP3Gain START: ' . count($files) . ' file(s)');
$handle = popen($params . ' 2>&1', 'r');
if ($handle !== false) {
    while (($line = fgets($handle)) !== false) {
        echo $line;
        flush();
        FppdLogLine('run_mp3gain.php', 'MediaOut', rtrim($line, "\n"));
    }
    $rc = pclose($handle);
} else {
    $rc = -1;
    echo "Failed to run mp3gain\n";
}
FppdLogLine('run_mp3gain.php', 'MediaOut', 'MP3Gain FINISH: (rc=' . $rc . ')');

printf("\n\nMP3Gain Complete...")
?>
