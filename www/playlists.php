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
                                {
                                    desc:$("#txtAddPlaylistDesc").val(),
                                    random:$("#randomizeAddPlaylist").val(),
                                },
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
        });
        $('.editPlaylistBtn').click(function(){
            $('.playlistEdit').fppDialog({
                title:'Edit Playlist Details',
                buttons: {
                    Save: {
                        click:function(){
                            SavePlaylist('', function(){
                                $('.playlistEdit').fppDialog('close');
                            });
                        },
                        class:'btn-success'
                    }
                }
            });
        })
        $('.playlistEditorHeaderTitleEditButton').click(function(){
            RenamePlaylist();
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
            $('.playlistSelectCount').html(playListArray.length);
            $.each(playListArray,function(i,playListName){
                var $playlistCol = $('<div class="col-md-4"/>');
                var $playlistCard = $('<div class="card has-shadow playlistCard"/>');
                var $playlistCardHeading = $('<h3>'+playListName+'</h3>');
              //  var $playlistEditButton = $('<button class="playlistCardEditButton circularButton circularEditButton playlistCardEditButton">Edit</button>');
                $playlistCol.append($playlistCard);
                $playlistCard.append($playlistCardHeading);
                //$playlistCard.append($playlistEditButton);
                $playlistCard.click(function(){
                    $('#playlistSelect').val(playListName).trigger('change');
                })
  
                $('.playlistSelectBody').append($playlistCol)
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
$activeParentMenuItem = 'content';
include 'menu.inc';
?>
    <div class="container">
        <h1 class="title">Playlists</h1>
        <div class='pageContent'>

            <div id="playlistEditor">


                <div class="playlistSelectContainer">
                    <div class="playlistSelectHeader">

                            <div class="row">
                                <div class="col-md">
                                    <h2>Your Playlists <div class="badge badge-light ml-1 playlistSelectCount"></div></h2>
                                    <select id='playlistSelect' class="hidden form-control form-control-lg form-control-rounded has-shadow" onChange='EditPlaylist();'>
                                    </select>
                                </div>
                                <div class="col-md-auto ml-lg-auto">
                                    <button class="playlistAddNewBtn buttons btn-outline-success btn-rounded  btn-icon-add"><i class="fas fa-plus"></i> New Playlist
                                    </button>
                                </div>
                            </div>

                    </div>
                 
                    <div class="playlistSelectBody row">
                        <div class="col-md-4 skeleton-loader"><div class="sk-block sk-card"></div></div>
                        <div class="col-md-4 skeleton-loader"><div class="sk-block sk-card"></div></div>
                        <div class="col-md-4 skeleton-loader"><div class="sk-block sk-card"></div></div>
                        <div class="col-md-4 skeleton-loader"><div class="sk-block sk-card"></div></div>
                        
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
                        <div>
                        <button class="playlistEditorBackButton has-shadow">
                        <i class="fas fa-chevron-left"></i><i class="fas fa-grip-horizontal"></i>
                        </button>
                        </div>

                        <div class="playlistEditorHeaderTitleWrapper">
                            <h2 class="playlistEditorHeaderTitle">
                            </h2>
                            <div class="playlistEditorHeaderTitleEditButtonWrapper">
                                <button class="circularButton circularEditButton playlistEditorHeaderTitleEditButton">Edit</button>
                            </div>
                        </div>
                        <div class="form-actions playlistEditorHeaderActions">
                            <button class="buttons editPlaylistBtn" >
                            <i class="fas fa-cog"></i>
                            </button>
                            <div class="dropdown pr-2">
                                <button class="buttons dropdown-toggle playlistEditButton" type="button" id="playlistEditMoreButton" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
                                    Actions
                                </button>
                                <div class="dropdown-menu playlistEditMoreButtonMenu" aria-labelledby="playlistEditMoreButton">
                                    
                                    <a href="#" value="Copy" onclick="CopyPlaylist();"  class="dropdown-item">Copy</a>
                                    <a href="#" value="Rename" onclick="RenamePlaylist();"  class="dropdown-item ">Rename</a>
                                    <a href="#" value="Randomize" onclick="RandomizePlaylistEntries();"  class="dropdown-item ">Randomize</a>
                                    <a href="#" value="Reset" onclick="EditPlaylist();"  class="dropdown-item ">Reset</a>
                                    <a href="#" value="Delete" onclick="DeletePlaylist();"  class="dropdown-item ">Delete</a>
                                </div>
                            </div>
                            <button class="buttons btn-success savePlaylistBtn" >
                                Save Playlist
                            </button>
                        </div>

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
                    <label for="randomizeAddPlaylist">Randomize:</label>
                    <select id='randomizeAddPlaylist' class="form-control">
                        <option value='0'>Off</option>
                        <option value='1'>Once per load</option>
                        <option value='2'>Every iteration</option>
                    </select>
                </div>

            </div>
            <div class="playlistEdit hidden">
                <div class="form-group hidden">
                    <label for="txtPlaylistName">Playlist Name:</label>
                    <input type="text" id="txtPlaylistName" class="pl_title form-control" />
                </div>
                <div class="form-group">
                    <label for="txtPlaylistDesc">Playlist Description:</label>
                    <input type="text" id="txtPlaylistDesc" class="pl_description form-control" />
                </div>
                <div class="form-group flow">
                    <label for="randomizePlaylist">Randomize:</label>
                    <select id='randomizePlaylist' class="form-control">
                        <option value='0'>Off</option>
                        <option value='1'>Once per load</option>
                        <option value='2'>Every iteration</option>
                    </select>
                </div>
                <div>
                    <? PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?>
                </div>
            </div>

        </div>
    </div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
