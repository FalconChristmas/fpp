<?
header( "Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

print("Running MP3Gain\n\n");

$files = json_decode(file_get_contents('php://input'));
$params = "mp3gain ";
$dir = GetDirSetting("music");
foreach ($files as $f) {
    $params = $params . " " . escapeshellarg($dir . "/" . $f);
}
system($params);

printf("\n\nMP3Gain Complete...")
?>