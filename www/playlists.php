<?php
$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
?>
<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once('playlistentry.php');
//require_once('pi_functions.php');

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

?>
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
</head>
<body onload="PopulateLists();">
<div id="bodyWrapper">
<?php
include 'menu.inc';
?>
<div style="width:1100px;margin:0 auto;"> <br/>
      <fieldset style="padding: 10px; border: 2px solid #000;">
    <legend>Playlists</legend>
    <!--    <div style="overflow: hidden; padding: 5px;">
-->
	<table><tr><td>
    <div style="float: left; width: 400px; padding: 5px; background: #f9f9f9; ; margin-left: 60px; border: 1px solid #ccc;  margin-top: 5px;">
          <form>
        New Playlist: <br/>
        <input id="txtNewPlaylistName" class="default-value" type="text" value="Enter Playlist Name" size="30" maxlength="32" onChange='AddNewPlaylist();'/>
        <input id="btnNew" onclick="AddNewPlaylist();" type="button" class="buttons" value="Add" />
      </form>
        </div>
	</td></tr>
	<tr><td><div id = "playList" style="float: left;"> </div></td></tr>
	</table>
	<input type='button' onClick='ConvertPlaylistsToJSON();' value='Convert FPP v1.x CSV Playlists to JSON'><br>
  </fieldset>
      <br/>
	  <a name='editor'></a>
<? include_once('playlistEditor.php'); ?>
    </div>
  <?php include 'common/footer.inc'; ?>
</div>
<div id="playlistConverter" title="Playlist Converter" style="display: none">
  <div id="playlistConverterText">
  </div>
</div>
</body>
</html>
