<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once('common.php');

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

?>
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
<?
if (isset($_GET['playlist'])) {
?>
<script>
function LoadInitialPlaylist() {
    $('#playlistSelect').val("<? echo $_GET['playlist']; ?>").trigger('change');
}
</script>
<?
}
?>
</head>
<body onload="PopulateLists(); <?
if (isset($_GET['playlist'])) {
    echo "LoadInitialPlaylist();";
}
?>">
<div id="bodyWrapper">
<?php
include 'menu.inc';
?>
    <div class='pageContent'> <br/>
        <fieldset style="padding: 10px; border: 2px solid #000;">
            <legend>Playlists</legend>
            <div class='playlistBoxLeft'>
                <b>Select Playlist to Edit:</b><br>
                <select id='playlistSelect' size='5' style='min-width: 150px;' onChange='EditPlaylist();'>
                </select><br>
            </div>
            <div class='playlistBoxRight'>
                <b>Create New Playlist:</b><br/>
                <input id="txtNewPlaylistName" class="default-value" type="text" value="Enter Playlist Name" size="40" maxlength="64" onChange='CreateNewPlaylist();'/>
                <input id="btnNew" onclick="CreateNewPlaylist();" type="button" class="buttons" value="Create" />
            </div>
            <div class='clear'></div>
        </fieldset>
        <br/>
        <a name='editor'></a>
<? include_once('playlistEditor.php'); ?>
    </div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
