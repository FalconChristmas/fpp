<!DOCTYPE html>
<html>
<?php
require_once('config.php');
?>
<head>
<?php	include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><? echo $pageTitle; ?></title>

<link  href="jquery/jQuery-Upload-File/css/uploadfile.min.css" rel="stylesheet">

<script src="jquery/jQuery-Form-Plugin/js/jquery.form.js"></script>
<script src="jquery/jQuery-Upload-File/js/jquery.uploadfile.min.js"></script>

<script type="text/javascript" src="jquery/Spin.js/spin.js"></script>
<script type="text/javascript" src="jquery/Spin.js/jquery.spin.js"></script>

<script>
    $(function() {
    $('#tblSequences').on('mousedown', 'tr', function(event,ui){
          $('#tblSequences tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          SequenceNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDownloadSequence','enable');
		  SetButtonState('#btnRenameSequence','enable');
		  SetButtonState('#btnDeleteSequence','enable');
    });

    $('#tblMusic').on('mousedown', 'tr', function(event,ui){
          $('#tblMusic tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          SongNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnPlayMusicInBrowser','enable');
		  SetButtonState('#btnDownloadMusic','enable');
		  SetButtonState('#btnRenameMusic','enable');
		  SetButtonState('#btnDeleteMusic','enable');
    });

    $('#tblVideos').on('mousedown', 'tr', function(event,ui){
          $('#tblVideos tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          VideoNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnSequenceConvertUpload','disable');
		  //SetButtonState('#btnPlayVideoInBrowser','enable');
		  SetButtonState('#btnVideoInfo','enable');
		  SetButtonState('#btnDownloadVideo','enable');
		  SetButtonState('#btnRenameVideo','enable');
		  SetButtonState('#btnDeleteVideo','enable');
    });

    $('#tblImages').on('mousedown', 'tr', function(event,ui){
          $('#tblImages tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          ImageNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnViewImage','enable');
		  SetButtonState('#btnDownloadImage','enable');
		  SetButtonState('#btnDeleteImage','enable');
    });

    $('#tblEffects').on('mousedown', 'tr', function(event,ui){
          $('#tblEffects tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          EffectNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnSequenceConvertUpload','disable');
		  SetButtonState('#btnDownloadEffect','enable');
		  SetButtonState('#btnRenameEffect','enable');
		  SetButtonState('#btnDeleteEffect','enable');
    });

    $('#tblScripts').on('mousedown', 'tr', function(event,ui){
          $('#tblScripts tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          ScriptNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnSequenceConvertUpload','disable');
		  SetButtonState('#btnViewScript','enable');
		  SetButtonState('#btnEditScript','enable');
		  SetButtonState('#btnRunScript','enable');
		  SetButtonState('#btnDownloadScript','enable');
		  SetButtonState('#btnCopyScript','enable');
		  SetButtonState('#btnRenameScript','enable');
		  SetButtonState('#btnDeleteScript','enable');
    });

    $('#tblLogs').on('mousedown', 'tr', function(event,ui){
          $('#tblLogs tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          LogFileSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnSequenceConvertUpload','disable');
		  SetButtonState('#btnViewLog','enable');
		  SetButtonState('#btnDownloadLog','enable');
		  SetButtonState('#btnDeleteLog','enable');
    });

    $('#tblUploads').on('mousedown', 'tr', function(event,ui){
          $('#tblUploads tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          UploadFileSelected  = $(this).find('td:first').text();
		  var extension = /\.(vix|xseq|lms|las|gled|seq|hlsidata)$/i;
		  // Disable LOR/LMS/HLS for now until fppconvert is enhanced
		  //extension = /\.(vix|xseq|gled|seq)$/i;
		  if ( UploadFileSelected.match(extension) )
			  SetButtonState('#btnSequenceConvertUpload','enable');
		  else
			  SetButtonState('#btnSequenceConvertUpload','disable');
		  SetButtonState('#btnDownloadUpload','enable');
		  SetButtonState('#btnDeleteUpload','enable');
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
  }

	function RunScript(scriptName)
	{
		window.open("runEventScript.php?scriptName=" + scriptName);
	}

	function EditScript(scriptName)
	{
		$('#fileText').html("Loading...");

		$.get("fppxml.php?command=getFile&dir=Scripts&filename=" + scriptName, function(text) {
			var ext = scriptName.split('.').pop();
			if (ext != "html")
			{
				var html = "<fieldset  class='fs'><legend> " + scriptName + " </legend><div><center><input type='button' class='buttons' onClick='SaveScript(\"" + scriptName + "\");' value='Save'> <input type='button' class='buttons' onClick='AbortScriptChange();' value='Cancel'><hr/>";
				html += "<textarea cols='100' rows='25' id='scriptText'>" + text + "</textarea></center></div></fieldset>";
				$('#fileText').html(html);
			}
		});

		$('#fileViewer').dialog({ height: 600, width: 800, title: "Script Editor" });
		$('#fileViewer').dialog( "moveToTop" );
	}

	function SaveScript(scriptName)
	{
		var contents = $('#scriptText').val();
		var info = {
				scriptName: scriptName,
				scriptBody: contents
			};

		postData = "command=saveScript&data=" + encodeURIComponent(JSON.stringify(info));

		$.post("fppjson.php", postData).done(function(data) {
			if (data.saveStatus == "OK")
			{
				$('#fileViewer').dialog('close');
				$.jGrowl("Script saved.");
			}
			else
			{
				DialogError("Save Failed", "Save Failed: " + data.saveStatus);
			}
		}).fail(function() {
			DialogError("Save Failed", "Save Failed!");
		});

	}

	function AbortScriptChange()
	{
		$('#fileViewer').dialog('close');
	}

</script>
<style>
#tblSequences {
	width: 100%;
}
#tblMusic {
	width: 100%;
}

#uploader_div {
	display: block;
	min-height: 200px;
	width: 100%;
	clear: both;
	padding-top: 00px;
}
fieldset {
	min-height: 200px;
	border: 2px solid #000000;
}
h2 {
	text-align: center;
}
.right {
	text-align: right;
}
.selectedentry {
	background: #888;
}
</style>
</head>

<body onload="GetAllFiles();">
<div id="bodyWrapper">
<?php	include 'menu.inc'; ?>
<div id="fileManager">
  <br />
  <div class='title'>File Manager</div>
  <div id="tabs">
    <ul>
      <li><a href="#tab-sequence">Sequences</a></li>
      <li><a href="#tab-audio">Audio</a></li>
      <li><a href="#tab-video">Video</a></li>
      <li><a href="#tab-images">Images</a></li>
      <li><a href="#tab-effects">Effects</a></li>
      <li><a href="#tab-scripts">Scripts</a></li>
      <li><a href="#tab-logs">Logs</a></li>
      <li><a href="#tab-uploads">Uploads</a></li>
    </ul>
    <div id="tab-sequence">
      <div id= "divSeq">
        <fieldset class="fs">
          <legend> Sequence Files (.fseq) </legend>
          <div id="divSeqData">
            <table id="tblSequences">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DownloadFile('Sequences', SequenceNameSelected);" id="btnDownloadSequence" class="disableButtons" type="button"  value="Download" />
            <input onclick= "RenameFile('Sequences', ScriptNameSelected);" id="btnRenameSequence" class="disableButtons" type="button"  value="Rename" />
            <input onclick="DeleteFile('Sequences', SequenceNameSelected);" id="btnDeleteSequence" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>Sequence files must be in the Falcon Player .fseq format and may be converted from various other sequencer formats using <a href='https://github.com/smeighan/xLights' target='_sequencer'>xLights</a> or <a href='https://github.com/pharhp/Light-Elf' target='_sequencer'>Light-Elf</a>.  <a href='https://github.com/smeighan/xLights' target='_sequencer'>xLights v4</a> uses .fseq as its native file format.  <a href='http://www.vixenlights.com' target='_sequencer'>Vixen 3</a> and recent versions of <a href='http://vixenplus.com/'>Vixen+</a> also have the ability to directly export .fseq files.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-audio">
      <div id= "divMusic">
        <fieldset  class="fs">
          <legend> Music Files (.mp3/.ogg/.m4a) </legend>
          <div id="divMusicData">
            <table id="tblMusic">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "PlayFileInBrowser('Music', SongNameSelected);" id="btnPlayMusicInBrowser" class="disableButtons" type="button"  value="Listen" />
            <input onclick= "DownloadFile('Music', SongNameSelected);" id="btnDownloadMusic" class="disableButtons" type="button"  value="Download" />
            <input onclick= "RenameFile('Music', ScriptNameSelected);" id="btnRenameMusic" class="disableButtons" type="button"  value="Rename" />
            <input onclick= "DeleteFile('Music', SongNameSelected);" id="btnDeleteMusic" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>Audio files must be in MP3, OGG, or M4A format.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-video">
      <div id= "divVideo">
        <fieldset  class="fs">
          <legend> Video Files (.mp4/.mkv) </legend>
          <div id="divVideoData">
            <table id="tblVideos">
            </table>
          </div>
          <hr />
          <div class='right'>
            <!--
            <input onclick= "PlayFileInBrowser('Videos', VideoNameSelected);" id="btnPlayVideoInBrowser" class="disableButtons" type="button"  value="View" />
            -->
            <input onclick= "GetVideoInfo(VideoNameSelected);" id="btnVideoInfo" class="disableButtons" type="button"  value="Video Info" />
            <input onclick= "DownloadFile('Videos', VideoNameSelected);" id="btnDownloadVideo" class="disableButtons" type="button"  value="Download" />
            <input onclick= "RenameFile('Videos', ScriptNameSelected);" id="btnRenameVideo" class="disableButtons" type="button"  value="Rename" />
            <input onclick= "DeleteFile('Videos', VideoNameSelected);" id="btnDeleteVideo" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>Video files must be in .mp4 or .mkv format.  H264 video is required for hardware acceleration on the Pi and AAC or MP3 audio are preferred.  Video playback is not currently supported on the BBB.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-images">
      <div id= "divImage">
        <fieldset  class="fs">
          <legend> Images </legend>
          <div id="divImageData">
            <table id="tblImages">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "ViewImage(ImageNameSelected);" id="btnViewImage" class="disableButtons" type="button"  value="View" />
            <input onclick= "DownloadFile('Images', ImageNameSelected);" id="btnDownloadImage" class="disableButtons" type="button"  value="Download" />
            <input onclick= "DeleteFile('Images', ImageNameSelected);" id="btnDeleteImage" class="disableButtons" type="button"  value="Delete" />
          </div>
        </fieldset>
      </div>
    </div>

    <div id="tab-effects">
      <div id= "divEffects">
        <fieldset  class="fs">
          <legend> Effect Sequences (.eseq) </legend>
          <div id="divEffectsData">
            <table id="tblEffects">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DownloadFile('Effects', EffectNameSelected);" id="btnDownloadEffect" class="disableButtons" type="button"  value="Download" />
            <input onclick= "RenameFile('Effects', ScriptNameSelected);" id="btnRenameEffect" class="disableButtons" type="button"  value="Rename" />
            <input onclick= "DeleteFile('Effects', EffectNameSelected);" id="btnDeleteEffect" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>Effects files are .fseq format files with an .eseq extension.  These special sequence files contain only the channels for a specific effect and always start at channel 1 in the sequence file.  The actual starting channel offset for the Effect is specified when you run it or configure the Effect in an Event.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-scripts">
      <div id= "divScripts">
        <fieldset  class="fs">
          <legend> Scripts (.sh/.pl/.pm/.php/.py)</legend>
          <div id="divScriptsData">
            <table id="tblScripts">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "ViewFile('Scripts', ScriptNameSelected);" id="btnViewScript" class="disableButtons" type="button"  value="View" />
            <input onclick= "RunScript(ScriptNameSelected);" id="btnRunScript" class="disableButtons" type="button"  value="Run" />
            <input onclick= "EditScript(ScriptNameSelected);" id="btnEditScript" class="disableButtons" type="button"  value="Edit" />
            <input onclick= "DownloadFile('Scripts', ScriptNameSelected);" id="btnDownloadScript" class="disableButtons" type="button"  value="Download" />
            <input onclick= "CopyFile('Scripts', ScriptNameSelected);" id="btnCopyScript" class="disableButtons" type="button"  value="Copy" />
            <input onclick= "RenameFile('Scripts', ScriptNameSelected);" id="btnRenameScript" class="disableButtons" type="button"  value="Rename" />
            <input onclick= "DeleteFile('Scripts', ScriptNameSelected);" id="btnDeleteScript" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>Scripts must have a .sh, .pl, .pm, .php, or .py extension.  Scripts may be executed inside an event.  These might be used in a show to trigger an external action such as sending a message to a RDS capable FM transmitter or a non-DMX/Pixelnet LED sign.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-logs">
      <div id= "divLogs">
        <fieldset  class="fs">
          <legend> Log Files </legend>
          <div id="divLogsData">
            <table id="tblLogs">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DownloadZip('Logs');" id="btnZipLogs" class="buttons" type="button" value="Zip" style="float: left;" />
            <input onclick= "ViewFile('Logs', LogFileSelected);" id="btnViewLog" class="disableButtons" type="button"  value="View" />
            <input onclick= "DownloadFile('Logs', LogFileSelected);" id="btnDownloadLog" class="disableButtons" type="button"  value="Download" />
            <input onclick= "DeleteFile('Logs', LogFileSelected);" id="btnDeleteLog" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>FPP logs may be viewed or downloaded for submission with bug reports.</font>
        </fieldset>
      </div>
    </div>

    <div id="tab-uploads">
      <div id= "divUploads">
        <fieldset  class="fs">
          <legend> Uploaded Files </legend>
          <div id="divUploadsData">
            <table id="tblUploads">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "ConvertFileDialog(UploadFileSelected);" id="btnSequenceConvertUpload" class="disableButtons" type="button"  value="Convert" style="float: left;"/>
            <input onclick= "DownloadFile('Uploads', UploadFileSelected);" id="btnDownloadUpload" class="disableButtons" type="button"  value="Download" />
            <input onclick= "DeleteFile('Uploads', UploadFileSelected);" id="btnDeleteUpload" class="disableButtons" type="button"  value="Delete" />
          </div>
          <br />
          <font size=-1>The upload directory is used as temporary storage when uploading media and sequencee files.  It is also used as permanent storage for other file formats which have no dedicated home.</font>
        </fieldset>
      </div>
    </div>
    <div id='fileUploader' class='ui-tabs-panel'>
      <fieldset class='fs'>
        <legend> Upload Files </legend>
        <div id='fileuploader'>Select Files</div>
      </fieldset>
    </div>
  </div>
</div>
<div id='fileViewer' title='File Viewer' style="display: none">
  <div id='fileText'>
  </div>
</div>
<div id="dialog-confirm" title="Sequence Conversion" style="display: none">
<p>Convert the selected file to?</p>
</div>
<div id="overlay">
</div>


<?php	include 'common/footer.inc'; ?>
<script>
	var activeTabNumber = 
<?php
	if (isset($_GET['tab']))
		print $_GET['tab'];
	else
		print "0";
?>;

    $("#tabs").tabs({cache: true, active: activeTabNumber, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

// jquery upload file
$(document).ready(function()
{
	$("#fileuploader").uploadFile({
		url:"jqupload.php",
		fileName:"myfile",
		multiple: true,
		autoSubmit: true,
		returnType: "json",
		doneStr: "Close",
		dragdropWidth: '95%',
		dragDropStr: "<span><b>Drag &amp; Drop or Select Files to upload</b></span>",
		onSuccess: function(files, data, xhr) {
			for (var i = 0; i < files.length; i++) {
				moveFile(files[i]);
			}
			GetAllFiles();
		},
		onError: function(files, status, errMsg) {
			alert("Error uploading file");
		},
		});
});

</script>
</body>
</html>
