<!DOCTYPE html>
<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
include 'common/menuHead.inc';

$advancedView = false;
if (isset($_GET['advancedView'])) {
	if ($_GET['advancedView'] == true || strtolower($_GET['advancedView'])  == "true") {
		$advancedView = true;
	} else {
		$advancedView = false;
	}
}
?>
<title><? echo $pageTitle; ?></title>
<script>
    var advancedView = <? echo $advancedView == true ? 'true' : 'false'; ?>;

    function updateMultiSyncRemotes(checkbox) {
		var remotes = "";

		if ($('#allRemotes').is(":checked")) {
			remotes = "255.255.255.255";

			$('input.remoteCheckbox').each(function() {
				if (($(this).is(":checked")) &&
						($(this).attr("name") != "255.255.255.255")) {
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
        var inp = document.getElementById("extraMultiSyncRemotes");
        if (inp && inp.value) {
            if (remotes != "") {
                remotes += ",";
            }
            var str = inp.value;
            str = str.replace(/\s/g, '');
            remotes += str;
        }
        
        
		$.get("fppjson.php?command=setSetting&key=MultiSyncRemotes&value=" + remotes
		).done(function() {
			settings['MultiSyncRemotes'] = remotes;
            if (remotes == "") {
                $.jGrowl("Remote List Cleared.  You must restart fppd for the changes to take effect.");
            } else {
                $.jGrowl("Remote List set to: '" + remotes + "'.  You must restart fppd for the changes to take effect.");
            }

            //Mark FPPD as needing restart
            $.get('fppjson.php?command=setSetting&key=restartFlag&value=1');
            settings['restartFlag'] = 1;
            //Get the resart banner showing
            CheckRestartRebootFlags();
        }).fail(function() {
			DialogError("Save Remotes", "Save Failed");
		});

	}
    function updateMultiSyncRemotesFromMulticast(checkbox) {
        if ($('#allRemotesMulticast').is(":checked")) {
            $('input.remoteCheckbox').each(function() {
                if (($(this).is(":checked")) &&
                    ($(this).attr("name") != "239.70.80.80")) {
                    $(this).prop('checked', false);
                }
            });
        }
        updateMultiSyncRemotes(checkbox);
    }


	function getFPPSystemInfo(ip, platform) {
        if (platform && !platform.includes("FPP") &&
            (platform.toLowerCase().includes("unknown")
             || platform == "xSchedule"
             || platform == "xLights"
             || platform.includes("Falcon ")
             || platform == "ESPixelStick")) {
            //eventually figure out what to do
            return;
        }
		$.get("http://" + ip + "/fppjson.php?command=getHostNameInfo", function(data) {
			$('#fpp_' + ip.replace(/\./g,'_') + '_desc').html(data.HostDescription);
		});
	}

	function getFPPSystemStatus(ip, platform) {
        if (platform && !platform.includes("FPP") &&
            (platform.toLowerCase().includes("unknown")
             || platform == "xSchedule"
             || platform == "xLights"
             || platform.includes("Falcon ")
             || platform == "ESPixelStick")) {
            //eventually figure out what to do
            return;
        }
		$.get("fppjson.php?command=getFPPstatus&ip=" + ip + (advancedView == true ? '&advancedView=true' : '')
		).done(function(data) {
			var status = 'Idle';
			var statusInfo = "";
			var elapsed = "";
			var files = "";

			if (data.status_name == 'playing') {
				status = 'Playing';

				elapsed = data.time_elapsed;

				if (data.current_sequence != "") {
					files += data.current_sequence;
					if (data.current_song != "")
						files += "<br>" + data.current_song;
				} else {
					files += data.current_song;
				}
			} else if (data.status_name == 'updating') {
				status = 'Updating';
			} else if (data.status_name == 'stopped') {
				status = 'Stopped';
			} else if (data.status_name == 'testing') {
				status = 'Testing';
            } else if (data.status_name == 'unreachable') {
                status = '<font color="red">Unreachable</font>';
            } else if (data.status_name == 'password') {
                status = '<font color="red">Protected</font>';
			} else if (data.status_name == 'unknown') {
                status = '-';
            } else if (data.status_name == 'idle') {
				if (data.mode_name == 'remote') {
					if ((data.sequence_filename != "") ||
						(data.media_filename != "")) {
						status = 'Syncing';

						elapsed += data.time_elapsed;

						if (data.sequence_filename != "") {
							files += data.sequence_filename;
							if (data.media_filename != "")
								files += "<br>" + data.media_filename;
						} else {
							files += data.media_filename;
						}
					}
				}
            } else {
                status = data.status_name;
            }

			var rowID = "fpp_" + ip.replace(/\./g, '_');
            var updatesAvailable = "";

			$('#' + rowID + '_status').html(status);
			$('#' + rowID + '_elapsed').html(elapsed);
			$('#' + rowID + '_files').html(files);
               
            if (data.warnings != null && data.warnings.length > 0) {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'table-row';
               var wHTML = "";
               for(var i = 0; i < data.warnings.length; i++) {
                wHTML += "<font color='red'>" + data.warnings[i] + "</font><br>";
               }
               $('#' + rowID + '_warningCell').html(wHTML);
            } else {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'none';
            }
               
			//Expert View Rows
            if(advancedView === true && data.status_name !== 'unknown' && data.status_name !== 'password') {
                $('#' + rowID + '_platform').html(data.advancedView.Platform + "<br><small class='hostDescriptionSM'>" + data.advancedView.Variant + "</small>");
                $('#advancedViewVersion_' + rowID).html(data.advancedView.Version);
                //$('#advancedViewBranch_' + rowID).html(data.advancedView.Branch);

                $('#advancedViewGitVersions_' + rowID).html("R: " + (typeof (data.advancedView.RemoteGitVersion) !== 'undefined' ? data.advancedView.RemoteGitVersion : 'Unknown') + "<br>L: " + (typeof (data.advancedView.LocalGitVersion) !== 'undefined' ? data.advancedView.LocalGitVersion : 'Unknown'));
                //Work out if there is a Git version difference
                if (((typeof (data.advancedView.RemoteGitVersion) !== 'undefined' && typeof (data.advancedView.LocalGitVersion) !== 'undefined')) && data.advancedView.RemoteGitVersion !== "Unknown") {
                    if (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion) {
                        updatesAvailable = '<a class="updateAvailable" href="http://' + ip + '/about.php" target="_blank">Update Available!</a>';
                    }
                }
                $('#advancedViewUpdates_' + rowID).html(updatesAvailable);

                $('#advancedViewUtilization_' + rowID).html("CPU: " + (typeof (data.advancedView.Utilization) !== 'undefined' ? Math.round(data.advancedView.Utilization.CPU) : 'Unk.') + "%" +
                    "<br>" +
                    "Mem: " + (typeof (data.advancedView.Utilization) !== 'undefined' ? Math.round(data.advancedView.Utilization.Memory) : 'Unk.') + "%" +
                    "<br>" +
                    "Uptime: " + (typeof (data.advancedView.Utilization) !== 'undefined' ? data.advancedView.Utilization.Uptime : 'Unk.'));
            }
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
            if (typeof remotes["255.255.255.255"] !== 'undefined') {
				star += " checked";
                delete remotes["255.255.255.255"];
            }
			star += " onClick='updateMultiSyncRemotes(this);'>";

			var newRow = "<tr>" +
				"<td align='center'>" + star + "</td>" +
				"<td>ALL Remotes Broadcast</td>" +
				"<td>255.255.255.255</td>" +
				"<td>ALL</td>" +
				"<td>Remote</td>" +
				"</tr>";
			$('#fppSystems tbody').append(newRow);
            
            var star = "<input id='allRemotesMulticast' type='checkbox' class='remoteCheckbox' name='239.70.80.80'";
            if (typeof remotes["239.70.80.80"] !== 'undefined') {
                star += " checked";
                delete remotes["239.70.80.80"];
            }
            star += " onClick='updateMultiSyncRemotesFromMulticast(this);'>";
            
            var newRow = "<tr>" +
            "<td align='center'>" + star + "</td>" +
            "<td>ALL Remotes Multicast</td>" +
            "<td>239.70.80.80</td>" +
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
                    if (typeof remotes[data[i].IP] !== 'undefined') {
						star += " checked";
                        delete remotes[data[i].IP];
                    }
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
				"<td>" + link + "<br><small class='hostDescriptionSM' id='fpp_" + ip.replace(/\./g,'_') + "_desc'>"+ hostDescription +"</small></td>" +
				"<td>" + data[i].IP + "</td>" +
                "<td id='" + rowID + "_platform'>" + data[i].Platform + "</td>" +
				"<td>" + fppMode + "</td>" +
				"<td id='" + rowID + "_status' align='center'></td>" +
				"<td id='" + rowID + "_elapsed'></td>" +
				"<td id='" + rowID + "_files'></td>";

            if (advancedView === true) {
                newRow = newRow + "<td class='advancedViewRowSpacer'></td>" +
                    "<td id='advancedViewVersion_" + rowID + "' class='advancedViewRow'></td>" +
                    //"<td id='advancedViewBranch_" + rowID + "'  class='advancedViewRow'></td>" +
                    "<td id='advancedViewGitVersions_" + rowID + "'  class='advancedViewRow'></td>" +
                    "<td id='advancedViewUpdates_" + rowID + "' class='advancedViewRow'></td>" +
                    "<td id='advancedViewUtilization_" + rowID + "'  class='advancedViewRow'></td>";
            }

            newRow = newRow + "</tr>";
			$('#fppSystems tbody').append(newRow);
            
            newRow = "<tr id='" + rowID + "_warnings' style='display:none'><td></td><td></td><td colspan='6' id='" + rowID + "_warningCell'></td></tr>";
            $('#fppSystems tbody').append(newRow);

			getFPPSystemStatus(ip, data[i].Platform);
			getFPPSystemInfo(ip, data[i].Platform);
		}
        var extras = "";
        for (var x in remotes) {
            if (extras != "") {
                extras += ",";
            }
            extras += x;
        }
        var inp = document.getElementById("extraMultiSyncRemotes");
        if (inp) {
            inp.value = extras;
        }
	}

	function getFPPSystems() {
		$('#masterLegend').hide();
		$('#fppSystems tbody').html("<tr><td colspan=5 align='center'>Loading system list from fppd.</td></tr>");

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
            <div class='fppTableWrapper<? if ($advancedView != true) { echo " fppTableWrapperAsTable"; }?>'>
                <div class='fppTableContents'>
			<table id='fppSystems' cellpadding='3'>
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
						if ($advancedView == true) {
							?>
                            <th class="advancedViewHeaderSpacer"></th>
                            <th class="advancedViewHeader">Version</th>
                            <!--<th class="advancedViewHeader">Branch</th> -->
                            <th class="advancedViewHeader">Git Version(s)</th>
                            <th class="advancedViewHeader">Updates</th>
                            <th class="advancedViewHeader">Utilization</th>
							<?php
						}
						?>
                    </tr>
				</thead>
				<tbody>
					<tr><td colspan=5 align='center'>Loading system list from fppd.</td></tr>
				</tbody>
			</table>
</div></div>
			<hr>
<?php
if ($settings['fppMode'] == 'master')
{
?>
			Additional MultiSync Remote IPs (comma separated): (For non-discoverable remotes)
            <input type="text" id="extraMultiSyncRemotes" maxlength="255" size="60" onchange='updateMultiSyncRemotes(null);' />

<br>
            CSV MultiSync Remote IP List (comma separated):
            <?
            $csvRemotes = "";
            if (isset($settings["MultiSyncCSVRemotes"])) {
                $csvRemotes = $settings["MultiSyncCSVRemotes"];
            }
            PrintSettingText("MultiSyncCSVRemotes", 1, 0, 255, 60, "", $csvRemotes); ?>
<br><br>
			<? PrintSettingCheckbox("Compress FSEQ files for transfer", "CompressMultiSyncTransfers", 0, 0, "1", "0"); ?> Compress FSEQ files during copy to Remotes to speed up file sync process<br>
<?php
}
?>
			<? PrintSettingCheckbox("Auto Refresh Systems Status", "MultiSyncRefreshStatus", 0, 0, "1", "0", "", "getFPPSystems"); ?> Auto Refresh status of FPP Systems<br>
			<? PrintSettingCheckbox("Avahi discovery", "AvahiDiscovery", 0, 0, "1", "0", "", "getFPPSystems"); ?> Enable Legacy FPP Avahi Discovery<br>
            <?php
                if ($advancedView ==true) {
					?>
                    <b style="color: #FF0000; font-size: 0.9em;">**Expert View Active - Auto Refresh is not recommended as it may cause slowdowns</b>
                    <br>
                    <b style="color: #FF0000; font-size: 0.9em;">**Git Versions : </b> <b style="color: #FF0000; font-size: 0.9em;">R: - Remote Git Version</b> | <b style="color: #FF0000; font-size: 0.9em;">L: - Local Git Version</b><br>
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
<?
	if ($advancedView == true)
		echo "<input type='button' class='buttons' value='Normal View' onclick=\"window.open('/multisync.php','_self')\">";
	else
		echo "<input type='button' class='buttons' value='Advanced View' onclick=\"window.open('/multisync.php?advancedView=true','_self')\">";
?>
<?php
if ($settings['fppMode'] == 'master')
{
?>
			<hr>
			<b>Copy Files from Master to Remotes</b><br>
<?
	if (!isset($settings['MultiSyncCopySequences']))
		$settings['MultiSyncCopySequences'] = 1;
?>
			<? PrintSettingCheckbox("Copy Sequences", "MultiSyncCopySequences", 0, 0, "1", "0"); ?> Copy Sequences<br>
			<? PrintSettingCheckbox("Copy Effects", "MultiSyncCopyEffects", 0, 0, "1", "0"); ?> Copy Effects<br>
			<? PrintSettingCheckbox("Copy Videos", "MultiSyncCopyVideos", 0, 0, "1", "0"); ?> Copy Videos<br>
			<? PrintSettingCheckbox("Copy Events", "MultiSyncCopyEvents", 0, 0, "1", "0"); ?> Copy Events<br>
			<? PrintSettingCheckbox("Copy Scripts", "MultiSyncCopyScripts", 0, 0, "1", "0"); ?> Copy Scripts<br>
			<input type='button' class='buttons' value='Copy Files' onClick='location.href="syncRemotes.php";'>
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

            //Mark FPPD as needing restart
            $.get('fppjson.php?command=setSetting&key=restartFlag&value=1');
            settings['restartFlag'] = 1;
            //Get the resart banner showing
            CheckRestartRebootFlags();
        }
	});

$(document).ready(function() {
	getFPPSystems();
});

</script>


</body>
</html>
