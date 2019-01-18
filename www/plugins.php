<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('fppversion.php');
include 'common/menuHead.inc';
?>
<script type="text/javascript" src="/js/fpp.js"></script>
<script type="text/javascript" src="/js/pluginList.js"></script>
<script type="text/javascript" src="/jquery/Spin.js/spin.js"></script>
<script type="text/javascript" src="/jquery/Spin.js/jquery.spin.js"></script>
<script>
var installedPlugins = [
<?
$handle = opendir($settings['pluginDirectory']);
if ($handle)
{
	while (($file = readdir($handle)) !== false)
	{
		if (!in_array($file, array('.', '..')))
		{
			if (file_exists($pluginDirectory."/".$file."/pluginInfo.json"))
				echo "    \"" . $file . "\",\n";
		}
	}
	closedir($handle);
}

?>
];

function PluginIsInstalled(plugin) {
	for (var i=0; i < installedPlugins.length; i++) {
		if (installedPlugins[i] == plugin)
			return 1;
	}

	return 0;
}

var firstInstalled = 1;
var firstCompatible = 1;
var firstIncompatible = 1;
function LoadPlugin(data) {
	var html = '';

	html += '<tr>';
	html += '<td><span class="pluginTitle">' + data.name + '</span></td>';
	html += '<td align="right">';

	var installed = PluginIsInstalled(data.repoName);
	var compatibleVersion = -1;
	for (var i = 0; i < data.versions.length; i++)
	{
		if (((data.versions[i].minFPPVersion == 0) ||
			 (data.versions[i].minFPPVersion <= getFPPVersionFloat())) &&
			((data.versions[i].maxFPPVersion == 0) ||
			 (data.versions[i].maxFPPVersion >= getFPPVersionFloat())))
		{
			compatibleVersion = i;
		}
	}

	if (installed)
	{
//		html += "<img src=\"images/update.png\" class=\"button\" title=\"Update Plugin\" onClick='updatePlugin(\"" + data.repoName + "\");'>";
		html += '</td><td align="right">';
		html += "<img src=\"images/uninstall.png\" class=\"button\" title=\"Uninstall Plugin\" onClick='uninstallPlugin(\"" + data.repoName + "\");'>";
	}
	else
	{
		html += '</td><td align="right">';
		if (compatibleVersion >= 0)
		{
			html += "<img src=\"images/install.png\" class=\"button\" title=\"Install Plugin\" onClick='installPlugin(\"" + data.repoName + "\", \"" + data.srcURL + "\", \"" + data.versions[compatibleVersion].branch + "\", \"" + data.versions[compatibleVersion].sha + "\");'>";
		}
	}

	html += '</td><td align="right">';
	html += '<a href="' + data.homeURL + '" target="_blank"><img src="images/home.png" title="Home Page" class="button"></a>';
	html += '</td><td align="right">';
	html += '<a href="' + data.srcURL + '" target="_blank"><img src="images/source.png" title="Source" class="button"></a>';
	html += '</td><td align="right">';
	html += '<a href="' + data.bugURL + '" target="_blank"><img src="images/bug.png" title="Report Bug" class="button"></a>';
	html += '</td></tr>';
	html += '<tr><td colspan="6">' + data.description;
	html += '<br><b>By:</b> ' + data.author;

	if (compatibleVersion == -1)
	{
		html += '<br><br>Plugin has compatible versions for FPP Versions:<br>';
		for (var i = 0; i < data.versions.length; i++)
		{
			if (i > 0)
				html += ',';

			if ((data.versions[i].minFPPVersion > 0) &&
				(data.versions[i].maxFPPVersion > 0))
				html += ' v' + data.versions[i].minFPPVersion + ' - v' + data.versions[i].maxFPPVersion;
			else if (data.versions[i].minFPPVersion > 0)
				html += ' &gt; v' + data.versions[i].minFPPVersion;
			else if (data.versions[i].maxFPPVersion > 0)
				html += ' &lt; v' + data.versions[i].maxFPPVersion;
		}
	}

	html += '</td></tr>';

	if (installed)
	{
		$('#installedPlugins').show();

		if (firstInstalled)
			firstInstalled = 0;
		else
			$('#installedPlugins').append('<tr><td colspan="6"><hr></td></tr>');

		$('#installedPlugins').append(html);

		if (compatibleVersion == -1)
			$('#installedPlugins').append('<tr><td colspan="6" class="bad">WARNING: This plugin is already installed, but may be incompatible with this FPP version.</td></tr>');
	}
	else if (compatibleVersion != -1)
	{
		if (firstCompatible)
			firstCompatible = 0;
		else
			$('#pluginTable').append('<tr><td colspan="6"><hr></td></tr>');

		$('#pluginTable').append(html);
	}
	else
	{
		$('#incompatiblePlugins').show();
		if (firstIncompatible)
			firstIncompatible = 0;
		else
			$('#incompatiblePlugins').append('<tr><td colspan="6"><hr></td></tr>');

		$('#incompatiblePlugins').append(html);
	}
}

function LoadPlugins() {
	// Display installed plugins first
	for (var i = 0; i < installedPlugins.length; i++)
	{
		var url = 'plugin.php?plugin=' + installedPlugins[i] + '&page=pluginInfo.json&nopage=1';
		$.ajax({
			url: url,
			dataType: 'json',
			success: function(data) {
				LoadPlugin(data);
			},
			fail: function() {
				alert('Error, failed to fetch ' + pluginList[i]);
			}
		});
	}

	// then display the remaining list
	for (var i = 0; i < pluginList.length; i++)
	{
		if (!PluginIsInstalled(pluginList[i][0]))
		{
			var url = pluginList[i][1];
			$.ajax({
				url: url,
				dataType: 'json',
				success: function(data) {
					LoadPlugin(data);
				},
				fail: function() {
					alert('Error, failed to fetch ' + pluginList[i]);
				}
			});
		}
	}
}

function ManualLoadInfo() {
	var url = $('#pluginInfoURL').val();

	if (url.indexOf('://') > -1)
	{
		$.ajax({
			url: url,
			dataType: 'json',
			success: function(data) {
				LoadPlugin(data);
			},
			fail: function() {
				alert('Error, failed to fetch ' + pluginInfos[i]);
			}
		});
	}
	else
	{
		alert('Invalid pluginInfo.json URL');
	}
}

$(document).ready(function() {
	LoadPlugins();
});
</script>
<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>

<div id=plugins" class="settings">
<fieldset>
<legend>Plugins</legend>

<table class='pluginTable' border=0 cellpadding=0>
<tbody id='pluginTableHead'>
	<tr><td colspan=6>To manually add a plugin not in the list, provide the URL for the pluginInfo.json file and click the button below:<br>
pluginInfo.json URL: <input id='pluginInfoURL' size=90 maxlength=255><br>
		<input type='button' onClick='ManualLoadInfo();' value='Retrieve Plugin Info'>
		</td></tr>
</tbody>
<tbody id='installedPlugins' style='display: none;'>
	<tr><td colspan=6>&nbsp;</td></tr>
	<tr><td colspan=6 class='pluginsHeader bgBlue'>Installed Plugins</td></tr>
</tbody>
<tbody id='pluginTable'>
	<tr><td colspan=6>&nbsp;</td></tr>
	<tr><td colspan=6 class='pluginsHeader bgGreen'>Available Plugins</td></tr>
</tbody>
<tbody id='incompatiblePlugins' style='display: none;'>
	<tr><td colspan=6>&nbsp;</td></tr>
	<tr><td colspan=6 class='pluginsHeader bgRed'>Incompatible Plugins</td></tr>
</tbody>
</table>

<div id="overlay">
</div>

</div>

<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
