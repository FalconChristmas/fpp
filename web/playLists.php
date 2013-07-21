<?php
require_once('playlistentry.php');
//require_once('pi_functions.php');
$a = session_id();

if(empty($a)) session_start();
{
	session_start();
}
$_SESSION['session_id'] = session_id();
ini_set('display_errors', 'On');
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
<?php
ini_set('display_errors', 'On');
error_reporting(E_ALL);



?>
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
  $(document).ready(function() {
    $("#sortable").sortable();
		$( "#sortable" ).sortable( "option", "axis", 'y' );
  
	});
	
  </script>
<script>

    $(function() {
    $('#sortable').sortable({
        start: function(event, ui) {
          var start_pos = ui.item.index();
          ui.item.data('start_pos', start_pos);
        },
				
        update: function(event, ui) {
            var start_pos = ui.item.data('start_pos');
        		var end_pos = ui.item.index();
						PlaylistEntryIndexChanged(end_pos,start_pos);
						var items = $('#sortable li');
	  				lastPlaylistEntry = items.index(this);

        }
    		});
		
		$('#sortable').on('mousedown', 'li', function(event,ui){
  				$('#sortable li:nth-child(1n)').removeClass('selectedEntry');
          $(this).addClass('selectedEntry');
					var items = $('#sortable li');
					lastPlaylistEntry = items.index(this);
					var j = 8+9;
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
<style>
#sortable {
	list-style-type: none;
	margin: 0;
	padding: 0;
	width: 100%;
	cursor: hand;
	cursor: pointer;
}
#sortable li {
	margin: 0 3px 2px;
	padding: 0 0.2em 1em 1.5em;
	font-size: 1.0em;
	height: 5px;
}
#sortable li span {
	position: absolute;
	margin-left: -1.3em;
}
.clear {
	clear: both;
}
ul.playlist {
	display: block;
	margin: 0;
	padding: 0;
}
ul.playlist li {
	float: left;
	list-style: none;
}
#playlistEntryProperties {
	width: 40%;
	background: rgb#FFF;
	float: left;
}
.buttons {
	text-align: center;
	float: left;
}
div.buttons input {
	width: 75px;
	margin-bottom: 10px;
	margin-top: 10px;
}
.items {
	width: 40%;
	background: rgb#FFF;
	float: right;
	margin: 0, auto;
}
.selectedEntry {
	background: gray;
	color: white;
}
.pl_title {
	font-size: larger;
}
h4, h3 {
	padding: 0;
	margin: 0;
}
div#playlistEntryProperties, div.items {
	padding: 10px;
}
.divButtons {
	float: left;
	text-align: center;
	width: 14%;
}
</style>
</head>

<body onload="PopulateLists();">
<div id="bodyWrapper">
  <?php	include 'menu.inc'; ?>
  <?php 
  function PrintMusicOptions()
  {
    foreach(scandir('/mnt/media/fpp/music') as $songFile) 
    {
      if($songFile != '.' && $songFile != '..')
      {
        echo "<option value='" . $songFile . "'>$songFile</option>";
      }
    }
  }			
  
  function PrintSequenceOptions()
  {
    foreach(scandir('/mnt/media/fpp/sequences') as $seqFile) 
    {
      if($seqFile != '.' && $seqFile != '..')
      {
        echo "<option value='" . $seqFile . "'>$seqFile</option>";
      }
    }
  }			
  
?>
  <div style="width:800px;margin:0 auto;"> <br/>
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>Playlists</legend>
      <div style="overflow: hidden; padding: 20px;">
      <div id = "playList" style="float: left;"> </div>
      <div style="float: left; width: 400px; padding: 10px; background: #f9f9f9; ; margin-left: 60px; border: 1px solid #ccc;  margin-top: 20px;">
        <form>
          New Playlist: <br/>
          <input id="txtNewPlaylistName" class="default-value" type="text" value="Enter Playlist Name" size="30" maxlength="32" />
          <br/>
          <input id="btnNew" onclick="AddNewPlaylist();" type="button" class="buttons" value="Add" />
        </form>
      </div>
    </fieldset>
    <br/>
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>Playlist Details</legend>
      <div>
        <input type="text" id="txtName" class="pl_title" />
        <p>Select details below and click 'Add'.</p>
      </div>
      <div id="playlistEntryProperties">
        <table width='100%'>
          <tr>
            <td>Type:</td>
            <td><select id="selType" size="1" onchange="PlaylistTypeChanged()">
                <option value = 'b'>Music and Sequence Data</option>
                <option value = 'm'>Music Only</option>
                <option value = 's'>Sequence Data Only</option>
                <option value = 'p'>Pause</option>
              </select></td>
          </tr>
          <tr id='row_music'>
            <td>Music:</td>
            <td><select id="selAudio" size="1">
                <?php PrintMusicOptions(); ?>
              </select></td>
          </tr>
          <tr id='row_sequence'>
            <td>Sequence: </td>
            <td><select id="selSequence" size="1">
                <?php PrintSequenceOptions(); ?>
              </select></td>
          </tr>
          <tr id='row_pause' style="display:none">
            <td>Pause:</td>
            <td><input type="text" id="txtPause" />
              (1-3600 secs.)</td>
          </tr>
        </table>
      </div>
      <div class="divButtons">
        <input id='btnAdd' width="200px" onclick="AddPlaylistEntry()" class="buttons" type="button" value="Add &raquo;" />
        <br />
        <input id='btnRemove'  width="200px"  onclick="RemoveEntry();" class="buttons" type="button" value="&laquo; Remove" />
      </div>
      <div class="items">
        <ul id="sortable">
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Music2.mp3 | Sequence1.seq</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 2</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 3</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 4</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 5</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 6</li>
          <li class="ui-state-default"><span class="ui-icon ui-icon-arrowthick-2-n-s"></span>Item 7</li>
        </ul>
      </div>
      <div class="clear"></div>
      <div style="margin-top: 20px; padding-top: 10px; border-top: 2px solid #000;">
        <input name="" type="button" value="Save" onclick="SavePlaylist();" class="buttons" />
        <input name="" type="button" value="Delete" onclick="DeletePlaylist();"  class="buttons"/>
      </div>
    </fieldset>
  </div>
</div>
  <?php	include 'common/footer.inc'; ?>
</body>
</html>
