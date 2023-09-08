<!DOCTYPE html>
<html>
<?php
require_once 'config.php';
?>
<head>
<?php	include 'common/menuHead.inc';?>


<?php
exec("df -k " . $mediaDirectory . "/upload |awk '/\/dev\//{printf(\"%d\\n\", $5);}'", $output, $return_val);
$freespace = $output[0];
unset($output);
?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><?echo $pageTitle; ?></title>


<script src="jquery/jQuery-Form-Plugin/js/jquery.form.js"></script>
<script src="js/filepond.min.js"></script>

<link rel="stylesheet" href="css/filepond.min.css" />
<style>
.fileponduploader {
    background: #fff;
    border: 2px dashed #ced4da;
    border-radius: 10px;
    transition: 0.3s all cubic-bezier(0.02, 0.72, 0.32, 0.99);
}
.filepond--root .filepond--drop-label {
    min-height: 100px;
    background: transparent;
}
.filepond--drop-label label {
    min-height: 100px;
}
.filepond--panel-root {
    background-color: transparent;
}
</style>

<script>
function GetVideoInfo(file) {
    $('#fileText').html("Getting Video Info.");

    $.get("api/media/" + file + "/meta", function (data) {
        DoModalDialog({
            id: "VideoViewer",
            title: "Video Info",
            class: "modal-lg modal-dialog-scrollable",
            body: '<pre>' + syntaxHighlight(JSON.stringify(data, null, 2)) + '</pre>',
            keyboard: true,
            backdrop: true,
            buttons: {
                "Close": function() {CloseModalDialog("VideoViewer");}
            }
        });
    });
}
function mp3GainProgressDialogDone() {
    $('#mp3GainProgressCloseButton').prop("disabled", false);
    EnableModalDialogCloseButton("mp3GainProgress");
}
function ButtonHandler(table, button) {
    var selectedCount = $('#tbl' + table + ' tr.selectedEntry').length;
    var filename = '';
    var filenames = [];
    if (selectedCount == 1) {
        filename = $('#tbl' + table + ' tr.selectedEntry').find('td:first').text();
    }

    if ((button == 'play') || (button == 'playHere')) {
        if (selectedCount == 1) {
            PlayPlaylist(filename, button == 'play' ? 1 : 0);
        } else {
            DialogError('Error', 'Error, unable to play multiple sequences at the same time.');
        }
    } else if (button == 'download') {
        var files = [];
        $('#tbl' + table + ' tr.selectedEntry').each(function() {
            files.push($(this).find('td:first').text());
        });
        DownloadFiles(table, files);
    } else if (button == 'rename') {
        if (selectedCount == 1) {
            RenameFile(table, filename);
        } else {
            DialogError('Error', 'Error, unable to rename multiple files at the same time.');
        }
    } else if (button == 'copyFile') {
        if (selectedCount == 1) {
            CopyFile(table, filename);
        } else {
            DialogError('Error', 'Error, unable to copy multiple files at the same time.');
        }
    } else if (button == 'delete') {
        $('#tbl' + table + ' tr.selectedEntry').each(function() {
            DeleteFile(table, $(this), $(this).find('td:first').text());
        });
    } else if (button == 'editScript') {
        if (selectedCount == 1) {
            EditScript(filename);
        } else {
            DialogError('Error', 'Error, unable to edit multiple files at the same time.');
        }
    } else if (button == 'playInBrowser') {
        if (selectedCount == 1) {
            PlayFileInBrowser(table, filename);
        } else {
            DialogError('Error', 'Error, unable to play multiple files at the same time.');
        }
    } else if (button == 'runScript') {
        if (selectedCount == 1) {
            RunScript(filename);
        } else {
            DialogError('Error', 'Error, unable to run multiple files at the same time.');
        }
    } else if (button == 'videoInfo') {
        if (selectedCount == 1) {
            GetVideoInfo(filename);
        } else {
            DialogError('Error', 'Error, unable to get info for multiple files at the same time.');
        }
    } else if (button == 'viewFile') {
        if (selectedCount == 1) {
            ViewFile(table, filename);
        } else {
            DialogError('Error', 'Error, unable to view multiple files at the same time.');
        }
    } else if (button == 'tailFile') {
        if (selectedCount == 1) {
            TailFile(table, filename, 50);
        } else {
            DialogError('Error', 'Error, unable to view multiple files at the same time.');
        }
    } else if (button == 'viewImage') {
        if (selectedCount == 1) {
            ViewImage(filename);
        } else {
            DialogError('Error', 'Error, unable to view multiple files at the same time.');
        }
    } else if (button == 'mp3gain') {
        var files = [];
        $('#tbl' + table + ' tr.selectedEntry').each(function() {
            files.push($(this).find('td:first').text());
        });
        var postData = JSON.stringify(files);
        DisplayProgressDialog("mp3GainProgress", "MP3Gain");
        StreamURL("run_mp3gain.php", 'mp3GainProgressText', 'mp3GainProgressDialogDone', 'mp3GainProgressDialogDone', 'POST', postData, 'application/json');
    }
}



function ClearSelections(table) {
    $('#tbl' + table + ' tr').removeClass('selectedEntry');
    DisableButtonClass('single' + table + 'Button');
    DisableButtonClass('multi' + table + 'Button');
}

function HandleMouseClick(event, row, table) {
    HandleTableRowMouseClick(event, row);

    var selectedCount = $('#tbl' + table + ' tr.selectedEntry').length;

    DisableButtonClass('single' + table + 'Button');
    DisableButtonClass('multi' + table + 'Button');

    if (selectedCount > 1) {
        EnableButtonClass('multi' + table + 'Button');
    } else if (selectedCount > 0) {
        EnableButtonClass('single' + table + 'Button');
    }
}

$(function() {
    $('#tblSequences').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Sequences');
    });

    $('#tblMusic').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Music');
    });

    $('#tblVideos').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Videos');
    });

    $('#tblImages').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Images');
    });

    $('#tblEffects').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Effects');
    });

    $('#tblScripts').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Scripts');
    });

    $('#tblLogs').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Logs');
    });

    $('#tblUploads').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Uploads');
    });

    $('#tblCrashes').on('mousedown', 'tr', function(event,ui){
        HandleMouseClick(event, $(this), 'Crashes');
    });

  });

  function GetAllFiles() {
	GetFiles('Sequences');
	GetFiles('Music');
	GetFiles('Videos');
	GetFiles('Images');
	GetFiles('Effects');
	GetFiles('Scripts');
	GetFiles('Logs');
	GetFiles('Uploads');
	GetFiles('Crashes');
  }

	function RunScript(scriptName)
	{
		window.open("runEventScript.php?scriptName=" + scriptName);
	}

	function EditScript(scriptName)
	{
        var options = {
            id: "scriptEditorDialog",
            title: "Script Editor : "+scriptName,
            body: "<div id='fileEditText' class='fileText'>Loading...</div>",
            footer: "",
            class: "modal-dialog-scrollable",
            keyboard: true,
            backdrop: true,
            buttons: {
                "Save": {
                    id: 'fileViewerCloseButton',
                    class: 'btn-success',
                    click:function(){
                      SaveScript($('#scriptText').data('scriptName'));
                    }
                },
                "Cancel": {
                    click: function() {
                        AbortScriptChange();
                    }
                }

            }
        };
        DoModalDialog(options);
        $.get("api/Scripts/" + scriptName, function(text) {
            var ext = scriptName.split('.').pop();
            if (ext != "html") {
                var html = "<textarea style='width: 100%' rows='25' id='scriptText'>" + text + "</textarea></center></div></fieldset>";
                $('#fileEditText').html(html);
                $('#scriptText').data('scriptName',scriptName);
            }
        });
    }
        
        
	function SaveScript(scriptName)
	{
		var contents = $('#scriptText').val();
        var url = 'api/scripts/' + scriptName;
		var postData = JSON.stringify(contents);

		$.post(url, postData).done(function(data) {
			if (data.status == "OK")
			{
                CloseModalDialog("scriptEditorDialog");
				$.jGrowl("Script saved.",{themeState:'success'});
			}
			else
			{
				DialogError("Save Failed", "Save Failed: " + data.status);
			}
		}).fail(function() {
			DialogError("Save Failed", "Save Failed!");
		});

	}

	function AbortScriptChange()
	{
        CloseModalDialog("scriptEditorDialog");
	}

</script>

</head>

<body onload="GetAllFiles();">
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'content';
include 'menu.inc';?>
  <div class="mainContainer">
<div class='title'>File Manager</div>
<?if ($freespace > 95) {?>
  <div class="alert alert-danger" role="alert">WARNING: storage device is almost full!</div>
    <?}?>
  <div class="pageContent">

    <div id="fileManager">

    <ul class="nav nav-pills pageContent-tabs" id="channelOutputTabs" role="tablist">
      <li class="nav-item">
        <a class="nav-link active" id="tab-sequence-tab" data-bs-toggle="pill" href="#tab-sequence" role="tab" aria-controls="tab-sequence" aria-selected="true">
          Sequences
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-audio-tab" data-bs-toggle="pill" href="#tab-audio" role="tab" aria-controls="tab-audio">
          Audio
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-video-tab" data-bs-toggle="pill" href="#tab-video" role="tab" aria-controls="tab-video">
        Video
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-images-tab" data-bs-toggle="pill" href="#tab-images" role="tab" aria-controls="tab-images">
        Images
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-effects-tab" data-bs-toggle="pill" href="#tab-effects" role="tab" aria-controls="tab-effects">
        Effects
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-scripts-tab" data-bs-toggle="pill" href="#tab-scripts" role="tab" aria-controls="tab-scripts">
        Scripts
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-logs-tab" data-bs-toggle="pill" href="#tab-logs" role="tab" aria-controls="tab-logs">
        Logs
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-uploads-tab" data-bs-toggle="pill" href="#tab-uploads" role="tab" aria-controls="tab-uploads">
        Uploads
        </a>
      </li>
      <li class="nav-item">
        <a class="nav-link " id="tab-crashes-tab" data-bs-toggle="pill" href="#tab-crashes" role="tab" aria-controls="tab-crashes">
        Crash Reports
        </a>
      </li>
    </ul>

      <div class="tab-content" id="fileUploadTabsContent">

        <div class="tab-pane fade show active" id="tab-sequence" role="tabpanel" aria-labelledby="tab-sequence-tab">
          <div id= "divSeq">
            <div class="backdrop">
                <h2> Sequence Files (.fseq) </h2>
                <div id="divSeqData" class="fileManagerDivData">
                  <table id="tblSequences">
                  </table>
                </div>

                <div class='form-actions'>
                  <input onclick="ClearSelections('Sequences');" class="buttons" type="button" value="Clear" />
                  <?php if (isset($settings['fppMode']) && ($settings['fppMode'] == 'player')) {?>
                  <input onclick="ButtonHandler('Sequences', 'play');" class="disableButtons singleSequencesButton" type="button"  value="Play" />
                  <input onclick="ButtonHandler('Sequences', 'playHere');" class="disableButtons singleSequencesButton" type="button"  value="Play Here" />
                  <?php }?>
                  <input onclick="ButtonHandler('Sequences', 'download');" class="disableButtons singleSequencesButton multiSequencesButton" type="button"  value="Download" />
                  <input onclick="ButtonHandler('Sequences', 'rename');" class="disableButtons singleSequencesButton" type="button"  value="Rename" />
                  <input onclick="ButtonHandler('Sequences', 'delete');" class="disableButtons singleSequencesButton multiSequencesButton" type="button"  value="Delete" />
                </div>
                <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
            </div>
          </div>
        </div>

        <div class="tab-pane fade" id="tab-audio" role="tabpanel" aria-labelledby="tab-audio-tab">
          <div id= "divMusic">

              <div class="backdrop">
                <h2> Music Files (.mp3/.ogg/.m4a/.flac/.aac) </h2>
                <div id="divMusicData" class="fileManagerDivData">
                  <table id="tblMusic">
                  </table>
                </div>

                <div class='form-actions'>
                  <input onclick="ClearSelections('Music');" class="buttons" type="button" value="Clear" />
                  <input onclick="ButtonHandler('Music', 'playInBrowser');" id="btnPlayMusicInBrowser" class="disableButtons singleMusicButton" type="button"  value="Listen" />
                  <? if (file_exists("/bin/mp3gain") || file_exists("/usr/bin/mp3gain") || file_exists("/opt/homebrew/bin/mp3gain") || file_exists("/usr/local/bin/mp3gain")) { ?>
                    <input onclick="ButtonHandler('Music', 'mp3gain');" id="btnPlayMusicInBrowser" class="disableButtons singleMusicButton multiMusicButton" type="button"  value="MP3Gain" />
                  <? } ?>

                  <input onclick="ButtonHandler('Music', 'download');" id="btnDownloadMusic" class="disableButtons singleMusicButton multiMusicButton" type="button"  value="Download" />
                  <input onclick="ButtonHandler('Music', 'rename');" id="btnRenameMusic" class="disableButtons singleMusicButton" type="button"  value="Rename" />
                  <input onclick="ButtonHandler('Music', 'delete');" id="btnDeleteMusic" class="disableButtons singleMusicButton multiMusicButton" type="button"  value="Delete" />
                </div>
                <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
              </div>

          </div>
        </div>

        <div class="tab-pane fade" id="tab-video" role="tabpanel" aria-labelledby="tab-video-tab">
          <div id= "divVideo">

              <div class="backdrop">
                <h2> Video Files (.mp4/.mkv) </h2>
                <div id="divVideoData" class="fileManagerDivData">
                  <table id="tblVideos">
                  </table>
                </div>

                <div class='form-actions'>
                  <input onclick="ClearSelections('Videos');" class="buttons" type="button" value="Clear" />
                  <input onclick="ButtonHandler('Videos', 'playInBrowser');" class="disableButtons singleVideosButton" type="button"  value="View" />
                  <input onclick="ButtonHandler('Videos', 'videoInfo');" class="disableButtons singleVideosButton" type="button"  value="Video Info" />
                  <input onclick="ButtonHandler('Videos', 'download');" class="disableButtons singleVideosButton multiVideosButton" type="button"  value="Download" />
                  <input onclick="ButtonHandler('Videos', 'rename');" class="disableButtons singleVideosButton" type="button"  value="Rename" />
                  <input onclick="ButtonHandler('Videos', 'delete');" class="disableButtons singleVideosButton multiVideosButton" type="button"  value="Delete" />
                </div>
                <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
              </div>

          </div>
        </div>

        <div class="tab-pane fade" id="tab-images" role="tabpanel" aria-labelledby="tab-images-tab">
          <div id= "divImage">

              <div class="backdrop">
                <h2> Images </h2>
                <div id="divImageData" class="fileManagerDivData">
                  <table id="tblImages">
                  </table>
                </div>

                <div class='form-actions'>
                  <input onclick="ClearSelections('Images');" class="buttons" type="button" value="Clear" />
                  <input onclick="ButtonHandler('Images', 'viewImage');" class="disableButtons singleImagesButton" type="button"  value="View" />
                  <input onclick="ButtonHandler('Images', 'download');" class="disableButtons singleImagesButton multiImagesButton" type="button"  value="Download" />
                  <input onclick="ButtonHandler('Images', 'rename');" class="disableButtons singleImagesButton" type="button"  value="Rename" />
                  <input onclick="ButtonHandler('Images', 'delete');" class="disableButtons singleImagesButton multiImagesButton" type="button"  value="Delete" />
                </div>
                <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
              </div>

          </div>
        </div>

        <div class="tab-pane fade" id="tab-effects" role="tabpanel" aria-labelledby="tab-effects-tab">
          <div id= "divEffects">

              <div class="backdrop">
                <h2> Effect Sequences (.eseq) </h2>
                <div id="divEffectsData" class="fileManagerDivData">
                  <table id="tblEffects">
                  </table>
                </div>

                <div class='form-actions'>
                  <input onclick="ClearSelections('Effects');" class="buttons" type="button" value="Clear" />
                  <input onclick="ButtonHandler('Effects', 'download');" class="disableButtons singleEffectsButton multiEffectsButton" type="button"  value="Download" />
                  <input onclick="ButtonHandler('Effects', 'rename');" class="disableButtons singleEffectsButton" type="button"  value="Rename" />
                  <input onclick="ButtonHandler('Effects', 'delete');" class="disableButtons singleEffectsButton multiEffectsButton" type="button"  value="Delete" />
                </div>
                <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
              </div>

          </div>
        </div>

        <div class="tab-pane fade" id="tab-scripts" role="tabpanel" aria-labelledby="tab-scripts-tab">
          <div id= "divScripts">
            <div class="backdrop">
              <legend> Scripts (.sh/.pl/.pm/.php/.py)</legend>
              <div id="divScriptsData" class="fileManagerDivData">
                <table id="tblScripts">
                </table>
              </div>

              <div class='form-actions'>
                <input onclick="ClearSelections('Scripts');" class="buttons" type="button" value="Clear" />
                <input onclick="ButtonHandler('Scripts', 'viewFile');" class="disableButtons singleScriptsButton" type="button"  value="View" />
                <input onclick="ButtonHandler('Scripts', 'runScript');" class="disableButtons singleScriptsButton" type="button"  value="Run" />
                <input onclick="ButtonHandler('Scripts', 'editScript');" class="disableButtons singleScriptsButton" type="button"  value="Edit" />
                <input onclick="ButtonHandler('Scripts', 'download');" class="disableButtons singleScriptsButton multiScriptsButton" type="button"  value="Download" />
                <input onclick="ButtonHandler('Scripts', 'copyFile');" class="disableButtons singleScriptsButton" type="button"  value="Copy" />
                <input onclick="ButtonHandler('Scripts', 'rename');" class="disableButtons singleScriptsButton" type="button"  value="Rename" />
                <input onclick="ButtonHandler('Scripts', 'delete');" class="disableButtons singleScriptsButton multiScriptsButton" type="button"  value="Delete" />
              </div>
              <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
            </div>
          </div>
        </div>

        <div class="tab-pane fade" id="tab-logs" role="tabpanel" aria-labelledby="tab-logs-tab">
          <div id= "divLogs">
            <div class="backdrop">
              <legend> Log Files </legend>
              <div id="divLogsData" class="fileManagerDivData">
                <table id="tblLogs">
                </table>
              </div>

              <div class='form-actions'>
                <input onclick="ClearSelections('Logs');" class="buttons" type="button" value="Clear" />
                <input onclick="DownloadZip('Logs');" class="buttons" type="button" value="Zip" />
                <input onclick="ButtonHandler('Logs', 'viewFile');" class="disableButtons singleLogsButton" type="button"  value="View" />
                <input onclick="ButtonHandler('Logs', 'tailFile');" class="disableButtons singleLogsButton" type="button"  value="Tail" />
                <input onclick="ButtonHandler('Logs', 'download');" class="disableButtons singleLogsButton multiLogsButton" type="button"  value="Download" />
                <input onclick="ButtonHandler('Logs', 'delete');" class="disableButtons singleLogsButton multiLogsButton" type="button"  value="Delete" />
              </div>
              <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
            </div>
          </div>
        </div>

        <div class="tab-pane fade" id="tab-uploads" role="tabpanel" aria-labelledby="tab-uploads-tab">
          <div id= "divUploads">
            <div class="backdrop">
              <legend> Uploaded Files </legend>
              <div id="divUploadsData" class="fileManagerDivData">
                <table id="tblUploads">
                </table>
              </div>

              <div class='form-actions'>
                <input onclick="ClearSelections('Uploads');" class="buttons" type="button" value="Clear" />
                <input onclick="ButtonHandler('Uploads', 'download');" class="disableButtons singleUploadsButton multiUploadsButton" type="button"  value="Download" />
                <input onclick="ButtonHandler('Uploads', 'copyFile');" class="disableButtons singleUploadsButton" type="button"  value="Copy" />
                <input onclick="ButtonHandler('Uploads', 'rename');" class="disableButtons singleUploadsButton" type="button"  value="Rename" />
                <input onclick="ButtonHandler('Uploads', 'delete');" class="disableButtons singleUploadsButton multiUploadsButton" type="button"  value="Delete" />
              </div>
              <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
            </div>
          </div>
        </div>
        <div class="tab-pane fade" id="tab-crashes" role="tabpanel" aria-labelledby="tab-crashes-tab">
          <div id= "divCrashes">
            <div class="backdrop">
              <legend> Crash Reports </legend>
              <div id="divUploadsData" class="fileManagerDivData">
                <table id="tblCrashes">
                </table>
              </div>

              <div class='form-actions'>
                <input onclick="ClearSelections('Crashes');" class="buttons" type="button" value="Clear" />
                <input onclick="ButtonHandler('Crashes', 'download');" class="disableButtons singleCrashesButton multiCrashesButton" type="button"  value="Download" />
                <input onclick="ButtonHandler('Crashes', 'delete');" class="disableButtons singleCrashesButton multiCrashesButton" type="button"  value="Delete" />
              </div>
              <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
            </div>
          </div>
        </div>
        <div id='fileponduploader' class='fileponduploader ui-tabs-panel'>
            <input type="file" class="filepond" id="filepondInput" multiple>
        </div>
      </div>
    </div>
    <div id="overlay">
    </div>
  </div>
</div>


<?php	include 'common/footer.inc';?>
<script>
	var activeTabNumber =
<?php
if (isset($_GET['tab'])) {
    print urlencode($_GET['tab']);
} else {
    print "0";
}

?>;

    $("#tabs").tabs({cache: true, active: activeTabNumber, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

    const pond = FilePond.create(
                document.querySelector('#filepondInput'),
                {
                    labelIdle: `<b style="font-size: 1.3em;">Drag & Drop or Select Files to upload</b><br><br><span class="btn btn-dark filepond--label-action" style="text-decoration:none;">Select Files</span><br>`,
                    server: 'api/file/upload',
                    credits: false,
                    chunkUploads: true,
                    chunkSize: 1024 * 1024 * 64,
                    chunkForce: true,
                    maxParallelUploads: 3,
                    labelTapToUndo: "Tap to Close",
                }
                );

    pond.on("processfile", (error, file) => {
        console.log("Process file: " + file.filename);
        moveFile(file.filename);
        setTimeout(function(){
            GetAllFiles();
        }, 100);
    });

</script>
</body>
</html>
