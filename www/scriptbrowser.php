<!DOCTYPE html>
<html>
<head>
<?php
require_once("config.php");
require_once("fppversion.php");
include 'common/menuHead.inc';

function normalize_version($version)
{
    // convert a version string like 2.7.1-2-dirty to "20701"
	$version = preg_replace('/[v]/', '', $version);
    $version = preg_replace('/-.*/', '', $version);
    $parts = explode('.', $version);
    while (count($parts) < 3) {
        array_push($parts, "0");
    }
    $number = 0;
    foreach ($parts as $part) {
        $val = intval($part);
        if ($val > 99) {
            $val = 99;
        }
        $number = $number * 100 + $val;
    }
	return $number;
}

$rfs_ver = normalize_version(getFPPVersionTriplet());
?>
<title><? echo $pageTitle; ?></title>
<script>

	function ViewRemoteScript(category, filename) {
		$('#helpText').html("Retrieving Script");
		$('#dialog-help').dialog({ height: 600, width: 800, title: "Script Viewer" });
		$('#dialog-help').dialog( "moveToTop" );

		$.get("fppxml.php?command=viewRemoteScript&category=" + category + "&filename=" + filename
		).done(function(data) {
			$('#helpText').html("<pre>" + data.replace(/</g, '&lt;').replace(/>/g, '&gt;') + "</pre>");
		}).fail(function() {
			$('#helpText').html("Error loading script contents from repository.");
		});
	}

  function InstallRemoteScript(category, filename) {
		$.get("fppxml.php?command=installRemoteScript&category=" + category + "&filename=" + filename
		).done(function() {
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

   	if (count($parts) > 4 && normalize_version($parts[4]) <= $rfs_ver)
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
