<script>
var playlistEntryTypes = {};
<?
$json = file_get_contents('playlistEntryTypes.json');

$json = trim(preg_replace('/\s+/', ' ', $json));

echo "playlistEntryTypes = " . $json . ";\n";

$playlistEntryTypes = json_decode($json, true);
?>
</script>
