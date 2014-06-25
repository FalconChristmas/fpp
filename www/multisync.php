<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<title>Falcon PI Player - FPP</title>
<?php

require_once("config.php");

?>
<script>
	function updateMultiSyncRemotes() {
		var remotes = "";
		$('input.remoteCheckbox').each(function() {
			if ($(this).is(":checked")) {
				if (remotes != "") {
					remotes += ",";
				}
				remotes += $(this).attr("name");
			}
		});

		$.get("fppjson.php?command=setSetting&key=MultiSyncRemotes&value=" + remotes
		).success(function() {
			settings['MultiSyncRemotes'] = remotes;
			$.jGrowl("Remote List Saved: '" + remotes + "'.  You must restart fppd for the changes to take effect.");
		}).fail(function() {
			DialogError("Save Remotes", "Save Failed");
		});
	}

	function parseFPPSystems(data) {
		$('#fppSystems tbody').empty();

		if (settings['fppMode'] == 'master') {
			$('#masterLegend').show();
		}

		var remotes = [];
		if (typeof settings['MultiSyncRemotes'] !== 'undefined') {
			var tarr = settings['MultiSyncRemotes'].split(',');
			for (var i = 0; i < tarr.length; i++) {
				remotes[tarr[i]] = 1;
			}
		}

		for (var i = 0; i < data.length; i++) {
			var star = "";
			var link = "";
			if (data[i].Local)
			{
				link = data[i].HostName;
				star = "*";
			} else {
				link = "<a href='http://" + data[i].IP + "/'>" + data[i].HostName + "</a>";
				if ((settings['fppMode'] == 'master') &&
						(data[i].fppMode == "remote"))
				{
					star = "<input type='checkbox' class='remoteCheckbox' name='" + data[i].IP + "'";
					if (typeof remotes[data[i].IP] !== 'undefined')
						star += " checked";
					star += " onClick='updateMultiSyncRemotes();'>";
				}
			}

			var fppMode = 'Player';
			if (data[i].fppMode == 'bridge')
				fppMode = 'Bridge';
			else if (data[i].fppMode == 'master')
				fppMode = 'Master';
			else if (data[i].fppMode == 'remote')
				fppMode = 'Remote';

			var newRow = "<tr>" +
				"<td align='center'>" + star + "</td>" +
				"<td>" + link + "</td>" +
				"<td>" + data[i].IP + "</td>" +
				"<td>" + data[i].Platform + "</td>" +
				"<td>" + fppMode + "</td>" +
				"</tr>";
			$('#fppSystems tbody').append(newRow);
		}
	}

	function getFPPSystems() {
		$('#masterLegend').hide();
		$('#fppSystems tbody').empty();
		$('#fppSystems tbody').append("<tr><td colspan=5 align='center'>Loading...</td></tr>");

		$.get("/fppjson.php?command=getSetting&key=MultiSyncRemotes", function(data) {
			settings['MultiSyncRemotes'] = data.MultiSyncRemotes;
			$.get("/fppjson.php?command=getFPPSystems", function(data) {
				parseFPPSystems(data);
			});
		});
	}

	function refreshFPPSystems() {
		setTimeout(function() { getFPPSystems(); }, 1000);
	}

</script>
<style>
#fppSystems{
	border: 1px;
}

.masterHeader{
	width: 15%;
}

.masterValue{
	width: 40%;
}

.masterButton{
	text-align: right;
	width: 25%;
}
</style>
</head>
<body>
<div id="bodyWrapper">
	<?php include 'menu.inc'; ?>
	<br/>
	<div id="uifppsystems" class="settings">
		<fieldset>
			<legend>Discovered FPP Systems</legend>
			<table id='fppSystems' cellspacing='5'>
				<thead>
					<tr>
						<th>&nbsp;</th>
						<th>System Name</th>
						<th>IP Address</th>
						<th>Platform</th>
						<th>Mode</th>
					</tr>
				</thead>
				<tbody>
					<tr><td colspan=5 align='center'>Loading...</td></tr>
				</tbody>
			</table>
			<hr>
			<font size=-1>
				<span id='legend'>
				* - Local System
				<span id='masterLegend' style='display:none'><br>&#x2713; - Sync Remote FPP with this Master instance</span>
				</span>
			</font>
			<br>
			<input type='button' class='buttons' value='Refresh' onClick='getFPPSystems();'>
			<input type='button' class='buttons' value='Restart FPPD' onClick='RestartFPPD();'>
		</fieldset>
	</div>
</div>
<?php include 'common/footer.inc'; ?>

<script>

$(document).ready(function() {
	getFPPSystems();
});

</script>


</body>
</html>
