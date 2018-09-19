<!DOCTYPE html>
<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
include 'common/menuHead.inc';

$expertView = false;
if (isset($_GET['expertView'])) {
	if ($_GET['expertView'] == true || strtolower($_GET['expertView'])  == "true") {
		$expertView = true;
	} else {
		$expertView = false;
	}
}
?>
<title><? echo $pageTitle; ?></title>
<script>
    var expertView = <? echo $expertView == true ? 'true' : 'false'; ?>;

    function updateMultiSyncRemotes(checkbox) {
		var remotes = "";

		if ($('#allRemotes').is(":checked")) {
			remotes = "255.255.255.255";

			$('input.remoteCheckbox').each(function() {
				if (($(this).is(":checked")) &&
						($(this).attr("name") != "255.255.255.255"))
				{
					$(this).prop('checked', false);
					if ($(checkbox).attr("name") != "255.255.255.255")
						DialogError("WARNING", "'All Remotes' is already checked.  Uncheck 'All Remotes' if you want to select individual FPP instances.");
				}
			});
		} else {
			$('input.remoteCheckbox').each(function() {
				if ($(this).is(":checked")) {
					if (remotes != "") {
						remotes += ",";
					}
					remotes += $(this).attr("name");
				}
			});
		}

		$.get("fppjson.php?command=setSetting&key=MultiSyncRemotes&value=" + remotes
		).done(function() {
			settings['MultiSyncRemotes'] = remotes;
			if (remotes == "")
				$.jGrowl("Remote List Cleared.  You must restart fppd for the changes to take effect.");
			else
				$.jGrowl("Remote List set to: '" + remotes + "'.  You must restart fppd for the changes to take effect.");
		}).fail(function() {
			DialogError("Save Remotes", "Save Failed");
		});

	}

	function getFPPSystemStatus(ip) {
		$.get("fppjson.php?command=getFPPstatus&ip=" + ip + (expertView == true ? '&expertView=true' : '')
		).done(function(data) {
			var status = 'Idle';
			var statusInfo = "";
			var elapsed = "";
			var files = "";

			if (data.status_name == 'playing')
			{
				status = 'Playing';

				elapsed = data.time_elapsed;

				if (data.current_sequence != "")
				{
					files += data.current_sequence;
					if (data.current_song != "")
						files += "<br>" + data.current_song;
				}
				else
				{
					files += data.current_song;
				}
			}
			else if (data.status_name == 'updating')
			{
				status = 'Updating';
			}
			else if (data.status_name == 'stopped')
			{
				status = 'Stopped';
			}
            else if (data.status_name == 'unknown')
            {
                status = '-';
                if(typeof (data.reason) !== 'undefined'){
                    DialogError("Get FPP System Status", "Get Status Failed for " + ip + "\n " + data.reason);
                }else{
                    DialogError("Get FPP System Status", "Get Status Failed for " + ip);
                }
            }
			else if (data.status_name == 'idle')
			{
				if (data.mode_name == 'remote')
				{
					if ((data.sequence_filename != "") ||
						(data.media_filename != ""))
					{
						status = 'Syncing';

						elapsed += data.time_elapsed;

						if (data.sequence_filename != "")
						{
							files += data.sequence_filename;
							if (data.media_filename != "")
								files += "<br>" + data.media_filename;
						}
						else
						{
							files += data.media_filename;
						}
					}
				}
			}

			var rowID = "fpp_" + ip.replace(/\./g, '_');

			$('#' + rowID + '_status').html(status);
			$('#' + rowID + '_elapsed').html(elapsed);
			$('#' + rowID + '_files').html(files);
			//Expert View Rows
            if(expertView === true && data.status_name !== 'unknown') {
                $('#' + rowID + '_platform').html(data.expertView.Platform + "<br><small class='hostDescriptionSM'>" + data.expertView.Variant + "</small>");
                $('#expertViewVersion_' + rowID).html(data.expertView.Version);
                $('#expertViewBranch_' + rowID).html(data.expertView.Branch);
                $('#expertViewGitVersions_' + rowID).html("R: " + (typeof (data.expertView.RemoteGitVersion) !== 'undefined' ? data.expertView.RemoteGitVersion : 'Unknown') + "<br>L: " + (typeof ( data.expertView.LocalGitVersion) !== 'undefined' ? data.expertView.LocalGitVersion : 'Unknown'));
                $('#expertViewAutoUpdateState_' + rowID).html((data.expertView.AutoUpdatesDisabled === true ? "Disabled" : "Enabled") + "<br>" + (
                    (typeof (data.expertView.RemoteGitVersion) !== 'undefined' && typeof (data.expertView.LocalGitVersion) !== 'undefined') && (data.expertView.RemoteGitVersion !== data.expertView.LocalGitVersion) ? '<a class="updateAvailable" href="http://' + ip + '/about.php" target="_blank">Update Available!</a>' : ''));
                $('#expertViewUtilization_' + rowID).html("CPU: " + (typeof (data.expertView.Utilization) !== 'undefined' ? Math.round((data.expertView.Utilization.CPU) * 100) : 'Unknown') + "%" +
                    "<br>" +
                    "Mem: " + (typeof (data.expertView.Utilization) !== 'undefined' ? Math.round(data.expertView.Utilization.Memory) : 'Unknown')+ "%" +
                    "<br>" +
                    "Uptime: " + (typeof (data.expertView.Utilization) !== 'undefined' ? data.expertView.Utilization.Uptime : 'Unknown'));
            }
		}).fail(function() {
			DialogError("Get FPP System Status", "Get Status Failed for " + ip);
		}).always(function() {
			if ($('#MultiSyncRefreshStatus').is(":checked"))
				setTimeout(function() {getFPPSystemStatus(ip);}, 1000);
		});
	}

	function parseFPPSystems(data) {
		$('#fppSystems tbody').empty();

		var remotes = [];
		if (typeof settings['MultiSyncRemotes'] === 'string') {
			var tarr = settings['MultiSyncRemotes'].split(',');
			for (var i = 0; i < tarr.length; i++) {
				remotes[tarr[i]] = 1;
			}
		}

		if (settings['fppMode'] == 'master') {
			$('#masterLegend').show();

			var star = "<input id='allRemotes' type='checkbox' class='remoteCheckbox' name='255.255.255.255'";
			if (typeof remotes["255.255.255.255"] !== 'undefined')
				star += " checked";
			star += " onClick='updateMultiSyncRemotes(this);'>";

			var newRow = "<tr>" +
				"<td align='center'>" + star + "</td>" +
				"<td>ALL Remotes</td>" +
				"<td>255.255.255.255</td>" +
				"<td>ALL</td>" +
				"<td>Remote</td>" +
				"</tr>";
			$('#fppSystems tbody').append(newRow);
		}

		for (var i = 0; i < data.length; i++) {
			var star = "";
			var link = "";
			var ip = data[i].IP;
            var hostDescription = data[i].HostDescription;

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

			var rowID = "fpp_" + ip.replace(/\./g, '_');

			var newRow = "<tr id='" + rowID + "'>" +
				"<td align='center'>" + star + "</td>" +
				"<td>" + link + "<br><small class='hostDescriptionSM'>"+ hostDescription +"</small></td>" +
				"<td>" + data[i].IP + "</td>" +
                "<td id='" + rowID + "_platform'>" + data[i].Platform + "</td>" +
				"<td>" + fppMode + "</td>" +
				"<td id='" + rowID + "_status' align='center'></td>" +
				"<td id='" + rowID + "_elapsed'></td>" +
				"<td id='" + rowID + "_files'></td>";

            if (expertView === true) {
                newRow = newRow + "<td class='expertViewRowSpacer'></td>" +
                    "<td id='expertViewVersion_" + rowID + "' class='expertViewRow'></td>" +
                    "<td id='expertViewBranch_" + rowID + "'  class='expertViewRow'></td>" +
                    "<td id='expertViewGitVersions_" + rowID + "'  class='expertViewRow'></td>" +
                    "<td id='expertViewAutoUpdateState_" + rowID + "' class='expertViewRow'></td>" +
                    "<td id='expertViewUtilization_" + rowID + "'  class='expertViewRow'></td>";
            }

            newRow = newRow + "</tr>";
			$('#fppSystems tbody').append(newRow);

			getFPPSystemStatus(ip);
		}
	}

	function getFPPSystems() {
		$('#masterLegend').hide();
		$('#fppSystems tbody').empty();
		$('#fppSystems tbody').append("<tr><td colspan=5 align='center'>Loading...</td></tr>");

		$.get("fppjson.php?command=getSetting&key=MultiSyncRemotes", function(data) {
			settings['MultiSyncRemotes'] = data.MultiSyncRemotes;
			$.get("fppjson.php?command=getFPPSystems", function(data) {
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
						<th id="hostnameColumn">Hostname</th>
						<th>IP Address</th>
						<th>Platform</th>
						<th>Mode</th>
						<th>Status</th>
						<th>Elapsed</th>
						<th>File(s)</th>
						<?php
                        //Only show expert view is requested
						if ($expertView == true) {
							?>
                            <th class="expertViewHeaderSpacer"></th>
                            <th class="expertViewHeader">Version</th>
                            <th class="expertViewHeader">Branch</th>
                            <th class="expertViewHeader">Git Version(s)</th>
                            <th class="expertViewHeader">Auto Updates</th>
                            <th class="expertViewHeader">Utilization</th>
							<?php
						}
						?>
                    </tr>
				</thead>
				<tbody>
					<tr><td colspan=5 align='center'>Loading...</td></tr>
				</tbody>
			</table>
			<hr>
<?php
if ($settings['fppMode'] == 'master')
{
?>
			CSV MultiSync Remote IP List (comma separated): (NOTE: Only used for F16v3 running in Remote mode)
			<? PrintSettingText("MultiSyncCSVRemotes", 1, 0, 255, 60, "", $settings["MultiSyncCSVRemotes"]); ?><br>
			<? PrintSettingCheckbox("Compress FSEQ files for transfer", "CompressMultiSyncTransfers", 0, 0, "1", "0"); ?> Compress FSEQ files during copy to Remotes to speed up file sync process<br>
<?php
}
?>
			<? PrintSettingCheckbox("Auto Refresh Systems Status", "MultiSyncRefreshStatus", 0, 0, "1", "0", "", "getFPPSystems"); ?> Auto Refresh status of FPP Systems<br>
            <?php
                if ($expertView ==true) {
					?>
                    <b style="color: #FF0000">**Expert View Active - Auto Refresh is not recommended as it may cause slowdowns</b>
					<?php
				}
            ?>
			<hr>
			<font size=-1>
				<span id='legend'>
				* - Local System
				<span id='masterLegend' style='display:none'><br>&#x2713; - Sync Remote FPP with this Master instance</span>
				</span>
			</font>
			<br>
            <input type='button' class='buttons' value='Refresh' onClick='getFPPSystems();'>
            <br>
            <br>
            <span><b>Views:</b></span>
            <br>
            <input type='button' class='buttons' value='Normal View'
                   onclick="window.open('/multisync.php','_self')">
            <br>
            <input type='button' class='buttons' value='Expert View'
                   onclick="window.open('/multisync.php?expertView=true','_self')">
<?php
if ($settings['fppMode'] == 'master')
{
?>
			<input type='button' class='buttons' value='Sync Files' onClick='location.href="syncRemotes.php";'>
<?php
}
?>
		</fieldset>
	</div>
	<?php include 'common/footer.inc'; ?>
</div>

<script>

$('#MultiSyncCSVRemotes').on('change keydown paste input', function()
	{
		var key = 'MultiSyncCSVRemotes';
		var desc = $('#' + key).val();
		if (settings[key] != desc)
		{
			$.get('fppjson.php?command=setSetting&key=' + key + '&value=' + desc);
			settings[key] = desc;
		}
	});

$(document).ready(function() {
	getFPPSystems();
});

</script>


</body>
</html>
