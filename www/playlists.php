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
    var initialPlaylist = "<? echo $_GET['playlist']; ?>";
</script>
<?
}
?>
<script>
    function LoadInitialPlaylist() {
        $('#playlistSelect').val(initialPlaylist).trigger('change');
    }
    $(function(){
        $('.playlistCreateContainer').hide();
        $('.playlistAddNewBtn').click(function(){
            $('.playlistCreateContainer').dialog({
                modal: true
            });

        })
        $('#btnNew').click(function(){
            $('.playlistCreateContainer').dialog("close");
        });
        PopulateLists();
        if (typeof initialPlaylist !== 'undefined') {
            LoadInitialPlaylist();
        }else{
            $('#playlistSelect').prepend('<option value="" disabled selected>Select a Playlist</option>');
        } 

    })
</script>
<style>
.ui-resizable-se { bottom: 20px; right: 25px; }
</style>
</head>
<body>
<div id="bodyWrapper">
<?php
include 'menu.inc';
?>
    <div class="container">
        <h1 class="title">Playlists</h1>
        <div class='pageContent'>
                <div class="d-flex playlistSelectContainer">
                    <div class="fx-1">
                         <select id='playlistSelect' class="form-control form-control-lg form-control-rounded has-shadow" onChange='EditPlaylist();'>
                        </select>
                    </div>
                    <div class="or-v">
                        <span>or</span>
                    </div>
                    <div>
                        <button class="playlistAddNewBtn buttons btn-success btn-rounded btn-lg btn-icon-add">
                            Add New
                        </button>
                    </div>
                </div>

                <div class="playlistCreateContainer">
                    <b>Enter new playlist name</b><br/>
                    <input id="txtNewPlaylistName" class="default-value form-control" type="text" value="Enter Playlist Name" size="40" maxlength="64" onChange='CreateNewPlaylist();'/>
                    <input id="btnNew" onclick="CreateNewPlaylist();" type="button" class="buttons" value="Create" />
                </div>
                <div class='clear'></div>

                <a name='editor'></a>

                <div class="playlistEditorPanel backdrop">
                    <? include_once('playlistEditor.php'); ?>
                </div>



        </div>
    </div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
