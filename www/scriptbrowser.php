<html>
<head>
<?php
require_once("config.php");
include 'common/menuHead.inc';

function normalize_version($version)
{
	$version = preg_replace('/0\.([0-9])\.0/', '0.$1', $version);
	$version = preg_replace('/[\.v]/', '', $version);
	$version = preg_replace('/-.*/', '', $version);
	$number = intval(trim($version));
	$number = ($number * 100) / (pow(10,(substr_count($version,'.'))));

	return $number;
}

if ($fppRfsVersion == "Unknown")
	$fppRfsVersion = "9999";
$rfs_ver = normalize_version($fppRfsVersion);

?>
<title><? echo $pageTitle; ?></title>
<script>

	function ViewRemoteScript(category, filename) {
		$('#helpText').html("Retrieving Script");
		$('#dialog-help').dialog({ height: 600, width: 800, title: "Script Viewer" });
		$('#dialog-help').dialog( "moveToTop" );

		$.get("fppxml.php?command=viewRemoteScript&category=" + category + "&filename=" + filename
		).success(function(data) {
			$('#helpText').html("<pre>" + data + "</pre>");
		}).fail(function() {
			$('#helpText').html("Error loading script contents from repository.");
		});
	}

  function InstallRemoteScript(category, filename) {
		$.get("fppxml.php?command=installRemoteScript&category=" + category + "&filename=" + filename
		).success(function() {
			$.jGrowl("Script installed.");
		}).fail(function() {
			DialogError("Install Script", "Install Failed");
		});
	}

</script>
</head>
<body>
<div id="bodyWrapper">
	<?php include 'menu.inc'; ?>
	<br/>
	<div id="uiscripts" class="settings">
		<fieldset>
			<legend>Script Repository</legend>
			<table id='fppScripts' cellspacing='5'>
				<tbody>
<?
$indexCSV = file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/index.csv");
$lines = explode("\n", $indexCSV);

$count = 0;

foreach ($lines as $line)
{
	if (preg_match("/^#/", $line))
		continue;

	$parts = explode(',', $line);

	if (count($parts) < 4)
		continue;

	if (normalize_version($parts[3]) > $rfs_ver)
		continue;

	if ($count > 0)
	{
?>
				<tr><td colspan='3'><hr></td></tr>
<?
	}
?>
				<tr><td rowspan='3' valign='top'><input type='button' class='buttons' value='Install' onClick='InstallRemoteScript("<? echo $parts[0]; ?>", "<? echo $parts[1]; ?>");'><br />
						<input type='button' class='buttons' value='View' onClick='ViewRemoteScript("<? echo $parts[0]; ?>", "<? echo $parts[1]; ?>");'></td>
						<td><b>Category:</b></td><td><? echo $parts[0]; ?></td>
				<tr><td><b>Filename:</b></td><td><? echo $parts[1]; ?></td></tr>
				<tr><td valign='top'><b>Description:</b></td><td><? echo $parts[2]; ?></td></tr>
<?
	$count++;
}
?>
				</tbody>
			</table>
			<hr>
			NOTE: Some scripts such as the Remote Control scripts may require editting to configure variables
			to be functional.  After installing a script from the Script Repository you can view and download
			the script from the Scripts tab in the <a href='uploadfile.php'>File Manager</a> screen.
		</fieldset>
	</div>
	<?php include 'common/footer.inc'; ?>
</div>

</body>
</html>
