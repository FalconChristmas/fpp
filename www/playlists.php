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
        $('#playlistSelect').on('change',function(){
            $('.playlistEditorHeaderTitle').html($(this).val());
        })
        $('.playlistAddNewBtn').click(function(){
            $('.playlistAdd').fppDialog({
                title:'Add a New Playlist',
                buttons:{
                    "Add Playlist":{
                        click: function(){
                            SavePlaylistAs(
                                $("#txtAddPlaylistName").val(),
                                {desc:$("#txtAddPlaylistDesc").val()},
                                function(){
                                    onPlaylistArrayLoaded();
                                    $('#playlistSelect').val($("#txtAddPlaylistName").val()).trigger('change');
                                    //LoadPlaylistDetails($("#txtAddPlaylistName").val())
                                    
                                    $('.playlistAdd').fppDialog('close');
                                }
                            )
                            //CreateNewPlaylist();
                        },
                        class:'btn-success'
                    }
                }
            });
        })
        $('.playlistEntriesAddNewBtn').click(function(){
            $('#playlistEntryProperties').fppDialog({
                title:'New Entry',
                buttons:{
                    "Add":{
                        click: function(){
                            AddPlaylistEntry();
                            $('#playlistEntryProperties').fppDialog('close');
                            //CreateNewPlaylist();
                        },
                        class:'btn-success'
                    }
                }
            });
        });
        $('.savePlaylistBtn').click(function(){
            SavePlaylist($('#txtPlaylistName').val())
        })
        $('.playlistEditorBackButton').click(function(){
            $('#playlistEditor').removeClass('hasPlaylistDetailsLoaded');
        })
        function onPlaylistArrayLoaded(){
            $('.playlistSelectBody').html('');
            $.each(playListArray,function(i,playListName){
                var $playlistCard = $('<div class="col-md-4"><div class="card playlistCard"><h3>'+playListName+'</h3></div></div>');
                $playlistCard.click(function(){
                    $('#playlistSelect').val(playListName).trigger('change');
                })
                $('.playlistSelectBody').append($playlistCard)
            })

        }
        PopulateLists({
            onPlaylistArrayLoaded:onPlaylistArrayLoaded
        });
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

            <div id="playlistEditor">


                <div class="playlistSelectContainer">
                    <div class="playlistSelectHeader">
                        <div class="fx-1">
                            <h2>Select a Playlist</h2>
                            <select id='playlistSelect' class="hidden form-control form-control-lg form-control-rounded has-shadow" onChange='EditPlaylist();'>
                            </select>
                        </div>
                        <div class="ml-auto">
                            <button class="playlistAddNewBtn buttons btn-success btn-rounded btn-lg btn-icon-add">
                                New Playlist
                            </button>
                        </div>
                    </div>
                 
                    <div class="playlistSelectBody row">

                    </div>




                </div>

                <div class="playlistCreateContainer hidden">
                    <b>Enter new playlist name</b><br/>
                    <input id="txtNewPlaylistName" class="default-value form-control" type="text" value="Enter Playlist Name" size="40" maxlength="64" onChange='CreateNewPlaylist();'/>
               
                </div>
                <div class='clear'></div>

                <a name='editor'></a>
                <div class="playlistEditorContainer">
                    <div class="playlistEditorHeader">
                        <button class="buttons playlistEditorBackButton">
                            Back
                        </button>
                        <h2 class="playlistEditorHeaderTitle">

                        </h2>
                    </div>
                    <div class="playlistEditorPanel backdrop">
                        <? include_once('playlistEditor.php'); ?>
                    </div>
                </div>


            </div>
            <div class="playlistAdd hidden">
                <div class="form-group">
                    <label for="txtAddPlaylistName">Playlist Name:</label>
                    <input type="text" id="txtAddPlaylistName" class="pl_title form-control" />
                </div>
                <div class="form-group">
                    <label for="txtAddPlaylistDesc">Playlist Description:</label>
                    <input type="text" id="txtAddPlaylistDesc" class="pl_description form-control" />
                </div>
                <div class="form-group flow">
                    <label for="randomizePlaylist">Randomize:</label>
                    <select id='randomizePlaylist' class="form-control">
                        <option value='0'>Off</option>
                        <option value='1'>Once per load</option>
                        <option value='2'>Every iteration</option>
                    </select>
                </div>

            </div>
        </div>
    </div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
