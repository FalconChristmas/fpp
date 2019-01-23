<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('fppversion.php');
include 'common/menuHead.inc';
?>
<script type="text/javascript" src="/js/fpp.js"></script>
<script type="text/javascript" src="/jquery/Spin.js/spin.js"></script>
<script type="text/javascript" src="/jquery/Spin.js/jquery.spin.js"></script>
<script>
var installedPlugins = [];
var pluginInfos = [];

function PluginIsInstalled(plugin) {
	for (var i=0; i < installedPlugins.length; i++) {
		if (installedPlugins[i] == plugin)
			return 1;
	}

	return 0;
}

function GetInstalledPlugins() {
	var url = 'api/plugin';
	$.ajax({
		url: url,
		dataType: 'json',
		success: function(data) {
			installedPlugins = data.installedPlugins;
			LoadInstalledPlugins();
			GetPluginList();
		},
		fail: function() {
			GetPluginList();
			alert('Error, failed to get list of installed plugins.');
		}
	});
}

function GetPluginList() {
	var url = 'https://raw.githubusercontent.com/FalconChristmas/fpp-pluginList/master/pluginList.json';
	$.ajax({
		url: url,
		dataType: 'json',
		success: function(data) {
			LoadPlugins(data.pluginList);
		},
		fail: function() {
			alert('Error, failed to get pluginList.json');
		}
	});
}

function CheckPluginForUpdates(plugin) {
	var url = 'api/plugin/' + plugin + '/updates';

	$.ajax({
		url: url,
		type: 'POST',
		dataType: 'json',
		success: function(data) {
			if (data.Status == 'OK')
			{
				if (data.updatesAvailable)
					$('#row-' + plugin).find('.updatesAvailable').show();
				else
					$.jGrowl('No updates available for ' + plugin);
			}
			else
				alert('ERROR: ' + data.Message);
		},
		fail: function() {
			alert('Error, API call failed when checking plugin for updates');
		}
	});
}

function UpgradePlugin(plugin) {
	var url = 'api/plugin/' + plugin + '/upgrade';

	$.ajax({
		url: url,
		type: 'POST',
		dataType: 'json',
		success: function(data) {
			if (data.Status == 'OK')
				$('#row-' + plugin).find('.updatesAvailable').hide();
			else
				alert('ERROR: ' + data.Message);
		},
		fail: function() {
			alert('Error, API call failed when upgrading plugin');
		}
	});
}

function InstallPlugin(plugin, branch, sha) {
	var url = 'api/plugin';
	var i = FindPluginInfo(plugin);

	if (i < -1)
	{
		alert('Could not find plugin ' + plugin + ' in pluginInfo cache.');
		return;
	}

	var pluginInfo = pluginInfos[i];
	pluginInfo['branch'] = branch;
	pluginInfo['sha'] = sha;

	var postData = JSON.stringify(pluginInfo);
	$.ajax({
		url: url,
		type: 'POST',
		contentType: 'application/json',
		data: postData,
		dataType: 'json',
		success: function(data) {
			if (data.Status == 'OK')
				location.reload(true);
			else
				alert('ERROR: ' + data.Message);
		},
		fail: function() {
			alert('Error, API call to install plugin failed');
		}
	});
}

function UninstallPlugin(plugin) {
	var url = 'api/plugin/' + plugin;
	$.ajax({
		url: url,
		type: 'DELETE',
		dataType: 'json',
		success: function(data) {
			if (data.Status == 'OK')
				location.reload(true);
			else
				alert('ERROR: ' + data.Message);
		},
		fail: function() {
			alert('Error, API call to uninstall plugin failed');
		}
	});
}

function FindPluginInfo(plugin) {
	for (var i = 0; i < pluginInfos.length; i++)
	{
		if (pluginInfos[i].repoName == plugin)
			return i;
	}

	return -1;
}

var firstInstalled = 1;
var firstCompatible = 1;
var firstIncompatible = 1;
function LoadPlugin(data) {
	var html = '';

	pluginInfos.push(data);

	html += '<tr id="row-' + data.repoName + '">';
	html += '<td><span class="pluginTitle">' + data.name + '</span></td>';
	html += '<td align="right">';

	var installed = PluginIsInstalled(data.repoName);
	var compatibleVersion = -1;
	for (var i = 0; i < data.versions.length; i++)
	{
		if ((CompareFPPVersions(data.versions[i].minFPPVersion, getFPPVersionFloatStr()) < 0) &&
			((data.versions[i].maxFPPVersion == "0") || (data.versions[i].maxFPPVersion == "0.0") ||
			 (CompareFPPVersions(data.versions[i].maxFPPVersion, getFPPVersionFloatStr()) > 0)))
		{
			compatibleVersion = i;
		}
	}

	if (installed)
	{
		if (!data.hasOwnProperty('allowUpdates') || data.allowUpdates)
		{
			html += "<span class='pendingSpan updatesAvailable'";
			if (!data.updatesAvailable)
				html += " style='display: none;'";

			html += "><table border=0 cellspacing=0 class='updateTable'><tr><td>Updates<br>Available</td><td>&nbsp;&nbsp;</td><td><img src=\"images/update.png\" class=\"button\" title=\"Update Plugin\" onClick='UpgradePlugin(\"" + data.repoName + "\");'></td></tr></table>";

			html += '</span>';
			html += '</td><td align="right">';

			html += "<img src=\"images/checkmark.png\" class=\"button\" title=\"Check for Updates\" onClick='CheckPluginForUpdates(\"" + data.repoName + "\");'>";
		}
		else
		{
			html += '</td><td align="right">';
		}

		html += '</td><td align="right">';
		html += "<img src=\"images/uninstall.png\" class=\"button\" title=\"Uninstall Plugin\" onClick='UninstallPlugin(\"" + data.repoName + "\");'>";
	}
	else
	{
		html += '</td><td align="right">';
		html += '</td><td align="right">';
		if (compatibleVersion >= 0)
		{
			html += "<img src=\"images/install.png\" class=\"button\" title=\"Install Plugin\" onClick='InstallPlugin(\"" + data.repoName + "\", \"" + data.versions[compatibleVersion].branch + "\", \"" + data.versions[compatibleVersion].sha + "\");'>";
		}
	}

	html += '</td><td align="right">';
	html += '<a href="' + data.homeURL + '" target="_blank"><img src="images/home.png" title="Home Page" class="button"></a>';
	html += '</td><td align="right">';
	html += '<a href="' + data.srcURL + '" target="_blank"><img src="images/source.png" title="Source" class="button"></a>';
	html += '</td><td align="right">';
	html += '<a href="' + data.bugURL + '" target="_blank"><img src="images/bug.png" title="Report Bug" class="button"></a>';
	html += '</td></tr>';
	html += '<tr><td colspan="7">' + data.description;
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
			$('#installedPlugins').append('<tr><td colspan="7"><hr></td></tr>');

		$('#installedPlugins').append(html);

		if (compatibleVersion == -1)
			$('#installedPlugins').append('<tr><td colspan="7" class="bad">WARNING: This plugin is already installed, but may be incompatible with this FPP version.</td></tr>');
	}
	else if (data.repoName == 'fpp-plugin-Template')
	{
		$('#templatePlugin').show();
		$('#templatePlugin').append(html);
	}
	else if (compatibleVersion != -1)
	{
		if (firstCompatible)
			firstCompatible = 0;
		else
			$('#pluginTable').append('<tr><td colspan="7"><hr></td></tr>');

		$('#pluginTable').append(html);
	}
	else
	{
		$('#incompatiblePlugins').show();
		if (firstIncompatible)
			firstIncompatible = 0;
		else
			$('#incompatiblePlugins').append('<tr><td colspan="7"><hr></td></tr>');

		$('#incompatiblePlugins').append(html);
	}
}

function LoadInstalledPlugins() {
	for (var i = 0; i < installedPlugins.length; i++)
	{
		var url = 'api/plugin/' + installedPlugins[i];
		$.ajax({
			url: url,
			dataType: 'json',
			success: function(data) {
				LoadPlugin(data);
			},
			fail: function() {
				alert('Error, failed to fetch ' + installedPlugins[i]);
			}
		});
	}
}

function LoadPlugins(pluginList) {
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
	GetInstalledPlugins();
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
	<tr><td colspan=7>If you do not see the plugin you are looking for, you can manually add a plugin to the list by providing the URL for the plugin's pluginInfo.json file below and clicking the 'Retrieve Plugin Info' button:<br>
pluginInfo.json URL: <input id='pluginInfoURL' size=90 maxlength=255><br>
		<input type='button' onClick='ManualLoadInfo();' value='Retrieve Plugin Info'>
		</td></tr>
</tbody>
<tbody id='installedPlugins' style='display: none;'>
	<tr><td colspan=7>&nbsp;</td></tr>
	<tr><td colspan=7 class='pluginsHeader bgBlue'>Installed Plugins</td></tr>
</tbody>
<tbody id='pluginTable'>
	<tr><td colspan=7>&nbsp;</td></tr>
	<tr><td colspan=7 class='pluginsHeader bgGreen'>Available Plugins</td></tr>
</tbody>
<tbody id='templatePlugin' style='display: none;'>
	<tr><td colspan=7>&nbsp;</td></tr>
	<tr><td colspan=7 class='pluginsHeader bgDarkOrange'>Template Plugin</td></tr>
</tbody>
<tbody id='incompatiblePlugins' style='display: none;'>
	<tr><td colspan=7>&nbsp;</td></tr>
	<tr><td colspan=7 class='pluginsHeader bgRed'>Incompatible Plugins</td></tr>
</tbody>
</table>

<div id="overlay">
</div>

</div>

<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
