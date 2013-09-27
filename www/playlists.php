<?php
require_once('config.php');
require_once('playlistentry.php');
//require_once('pi_functions.php');

$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);
SetSetting("settings.xml","mysetting","hellow2");



function SetSetting($file,$varName,$varValue)
{
	if (file_exists($file)) 
	{
	
		$xml = simplexml_load_file($file);
		echo "Exists\n";
		//print_r($xml);
		if (property_exists($xml, $varName))
		{	
			echo "Netowrk Exist\n";
			$xml->$varName = $varValue;	
			print_r($xml);
		}
		else
		{
			$var_node = $xml->addchild($varName,$varValue);
		}
		$xml->asXML($file);
	}
	
}




?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
    <head>
    <?php	include 'common/menuHead.inc'; ?>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <link rel="stylesheet" href="http://code.jquery.com/ui/1.10.2/themes/smoothness/jquery-ui.css" />
    <script type="text/javascript" src="js/fpp.js"></script>
    <script src="http://code.jquery.com/jquery-migrate-1.1.1.min.js"></script>
    <script src="http://code.jquery.com/jquery-1.9.1.js"></script>
    <script src="http://code.jquery.com/ui/1.10.2/jquery-ui.js"></script>
    <script language="Javascript">
$(document).ready(function() {
$('.default-value').each(function() {
var default_value = this.value;
$(this).focus(function() {
if(this.value == default_value) {
this.value = '';
$(this).css('color', '#333');
}
});
$(this).blur(function() {
if(this.value == '') {
$(this).css('color', '#999');
this.value = default_value;
}
});
});
});
</script>
    <script>
$(document).ready(function () {
    //make table rows sortable
    $('#tblCreatePlaylistEntries tbody').sortable({
        start: function (event, ui) {
	          start_pos = ui.item.index();
            
        },
				update: function(event, ui) {
            var end_pos = ui.item.index();
						PlaylistEntryIndexChanged(end_pos,start_pos);
						var pl = document.getElementById("txtPlaylistName").value;
						PopulatePlayListEntries(pl,false,end_pos);
        },
        beforeStop: function (event, ui) {
            //undo the firefox fix.
            if (navigator.userAgent.toLowerCase().match(/firefox/) && ui.offset !== undefined) {
                $(window).unbind('scroll.sortableplaylist');
                ui.helper.css('margin-top', 0);
            }
        },
        helper: function (e, ui) {
            ui.children().each(function () {
                $(this).width($(this).width());
            });
            return ui;
        },
        scroll: true,
        stop: function (event, ui) {
            //SAVE YOUR SORT ORDER                    
        }
    }).disableSelection();
});

</script>
    <script>
    $(function() {
		$('#tblCreatePlaylistEntries').on('mousedown', 'tr', function(event,ui){
					$('#tblCreatePlaylistEntries tr').removeClass('selectedEntry');
          $(this).addClass('selectedEntry');
					var items = $('#tblCreatePlaylistEntries tr');
					lastPlaylistEntry  = items.index(this);
					
		});
	});
</script>
    <script type="text/javascript">
        $(function() {
            $('#txtNewPlaylistName').on('focus',function() {
                $(this).select();
            });
        });
    </script>
    <title>Playlist Manager</title>
    </head>

    <body onload="PopulateLists();">
<div id="bodyWrapper">
      <?php	include 'menu.inc'; ?>
<?php 
  function PrintMusicOptions()
  {
	  global $musicDirectory;
		echo "<select id=\"selAudio\" size=\"1\">";
    foreach(scandir($musicDirectory) as $songFile) 
    {
      if($songFile != '.' && $songFile != '..')
      {
        echo "<option value=\"" . $songFile . "\">" . $songFile . "</option>";
      }
    }
		echo "</select>";
  }			
  
  function PrintSequenceOptions()
  {
	  global $sequenceDirectory;
		echo "<select id=\"selSequence\" size=\"1\">";
    foreach(scandir($sequenceDirectory) as $seqFile) 
    {
      if($seqFile != '.' && $seqFile != '..' && !preg_match('/.eseq$/', $seqFile))
      {
        echo "<option value=\"" . $seqFile . "\">" . $seqFile . "</option>";
      }
    }
		echo "</select>";
  }			
  
?>
<div style="width:800px;margin:0 auto;"> <br/>
      <fieldset style="padding: 10px; border: 2px solid #000;">
    <legend>Playlists</legend>
    <!--    <div style="overflow: hidden; padding: 5px;">
-->
    <div id = "playList" style="float: left;"> </div>
    <div style="float: left; width: 400px; padding: 5px; background: #f9f9f9; ; margin-left: 60px; border: 1px solid #ccc;  margin-top: 5px;">
          <form>
        New Playlist: <br/>
        <input id="txtNewPlaylistName" class="default-value" type="text" value="Enter Playlist Name" size="30" maxlength="32" />
        <input id="btnNew" onclick="AddNewPlaylist();" type="button" class="buttons" value="Add" />
      </form>
        </div>
  </fieldset>
      <br/>
      <fieldset style="padding: 10px; border: 2px solid #000;">
    <legend>Playlist Details</legend>
    <div style="border-bottom:solid 1px #000; padding-bottom:10px;">
      <div style="float:left">
            <input type="text" id="txtPlaylistName" class="pl_title" />
            <br />
            <span style="font-size:10px; padding-top:10px; font-family:Arial">(To rename edit name and click 'Save') </span> </div>
      <div style="float:left">
            <input id="chkFirst" name="chkFirst" type="checkbox" value="first" style="margin-left:20px" onclick="SetPlayListFirst();"/>
            Play first entry only once<br />
            <input id="chkLast"  name="chkLast" type="checkbox" value="last" style="margin-left:20px"  onclick="SetPlayListLast();"/>
            Play last entry only once </div>
      <div style="float:left">
            <input name="" type="button" value="Save" onclick="SavePlaylist();" class="buttons" style="margin-left:50px"/>
            <input name="" type="button" value="Delete" onclick="DeletePlaylist();"  class="buttons" />
          </div>
          <div class="clear"></div>
    </div>
		    <br />
    <div id="playlistEntryProperties">
          <ul>
        <li> Type<br />
              <select id="selType" size="1" onchange="PlaylistTypeChanged()">
            <option value = 'b'>Music and Sequence</option>
            <option value = 'm'>Music Only</option>
            <option value = 's'>Sequence Only</option>
            <option value = 'p'>Pause</option>
          </select>
            </li>
        <li id="musicOptions">Music<br />
              <?php PrintMusicOptions();?> </li>
        <li id="sequenceOptions">Sequence<br />
              <?php PrintSequenceOptions();?> </li>
        <li id="pauseTime" style="display:none;">Pause Time<br />
              <input id="txtPause" name="txtPause" type="text" size="10" maxlength="10"/>
              (Seconds) </li>
        <li>
              <input id='btnAddPlaylistEntry'  width="200px"  onclick="AddPlaylistEntry();" class="buttons" type="button" value="Add" />
              <input id='btnRemovePlaylistEntry'  width="200px"  onclick="RemovePlaylistEntry();" class="buttons" type="button" value="Remove" />
            </li>
      </ul>
          <div class="clear"></div>
        </div>
    <div id="createPlaylistItems">
          <table id="tblCreatePlaylist">
        <tr id="rowCreatePlaylistHeader">
              <td width="5%" id="colPlaylistNumber" class="textRight">#</td>
              <td width="42%">Media File/Pause</td>
              <td width="42%">Sequence</td>
              <td width="11%">First/Last</td>
            </tr>
      </table>
          <table id="tblCreatePlaylistEntries" width="100%">
            <tbody id="tblCreatePlaylistEntries_tbody">
            </tbody>
      </table>
        </div>
    <span style="font-size:12px; font-family:Arial; margin-left:15px;">(Drag entry to reposition) </span>
  </fieldset>
    </div>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
