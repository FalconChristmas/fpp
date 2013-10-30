<?php
require_once('config.php');
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php	include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Files</title>
<link rel="stylesheet" href="css/theme/jquery-ui-1.10.3.custom.css" />
<script type="text/javascript" src="js/fpp.js"></script>
<script src="js/jquery-1.9.1.js"></script>
<script src="js/jquery-ui.js"></script>
<script src="js/upload/ajaxupload.js" type="text/javascript"></script>
<link rel="stylesheet" href="css/fpp.css" />
<link rel="stylesheet" href="css/classicTheme/style.css" type="text/css" media="all" />
<script>
    $(function() {
    $('#tblSequences').on('mousedown', 'tr', function(event,ui){
          $('#tblSequences tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          SequenceNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDeleteSequence','enable');
    });

    $('#tblMusic').on('mousedown', 'tr', function(event,ui){
          $('#tblMusic tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          SongNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDeleteMusic','enable');
    });

    $('#tblVideos').on('mousedown', 'tr', function(event,ui){
          $('#tblVideos tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          VideoNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDeleteVideo','enable');
    });

    $('#tblEffects').on('mousedown', 'tr', function(event,ui){
          $('#tblEffects tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          EffectNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDeleteEffect','enable');
    });

    $('#tblScripts').on('mousedown', 'tr', function(event,ui){
          $('#tblScript tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          ScriptNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDeleteScript','enable');
    });

    $('#tblLogs').on('mousedown', 'tr', function(event,ui){
          $('#tblLogs tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          LogFileSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnViewLog','enable');
		  SetButtonState('#btnDownloadLog','enable');
    });

  });
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
	height: 100%;
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

<body onload="GetFiles('Sequences'); GetFiles('Music'); GetFiles('Videos'); GetFiles('Effects'); GetFiles('Scripts'); GetFiles('Logs');">
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
      <li><a href="#tab-effects">Effects</a></li>
      <li><a href="#tab-scripts">Scripts</a></li>
	  <!-- hide Logs tab for now until we get logs in the right directory -->
      <li style="display:none"><a href="#tab-logs">Logs</a></li>
      </li>
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
            <input onclick="DeleteSequence();" id="btnDeleteSequence" class="disableButtons" type="button"  value="Delete" />
          </div>
        </fieldset>
      </div>
    </div>
    <div id="tab-audio">
      <div id= "divMusic">
        <fieldset  class="fs">
          <legend> Music Files (.ogg) </legend>
          <div id="divMusicData">
            <table id="tblMusic">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DeleteMusic();" id="btnDeleteMusic" class="disableButtons" type="button"  value="Delete" />
          </div>
        </fieldset>
      </div>
    </div>
    <div id="tab-video">
      <div id= "divVideo">
        <fieldset  class="fs">
          <legend> Video Files (.mp4) </legend>
          <div id="divVideoData">
            <table id="tblVideos">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DeleteVideo();" id="btnDeleteVideo" class="disableButtons" type="button"  value="Delete" />
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
            <input onclick= "DeleteEffect();" id="btnDeleteEffect" class="disableButtons" type="button"  value="Delete" />
          </div>
        </fieldset>
      </div>
    </div>
    <div id="tab-scripts">
      <div id= "divScripts">
        <fieldset  class="fs">
          <legend> Scripts (.sh)</legend>
          <div id="divScriptsData">
            <table id="tblScripts">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick= "DeleteScript();" id="btnDeleteScript" class="disableButtons" type="button"  value="Delete" />
          </div>
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
            <input onclick= "ViewLog();" id="btnViewLog" class="disableButtons" type="button"  value="View" />
            <input onclick= "DownloadLog();" id="btnDownloadLog" class="disableButtons" type="button"  value="Download" />
          </div>
        </fieldset>
      </div>
    </div>
  </div>
	<hr />
  <br />
  <div id="uploader_div" > </div>
  <script type="text/javascript">
$('#uploader_div').ajaxupload({
	url:'upload.php',
	remotePath:'<?php global $mediaDirectory; echo $mediaDirectory; ?>upload/',
	removeOnSuccess: true,
	maxFileSize:'10000M',
	chunkSize:1048576,
	success:	function(fileName){
				moveFile(fileName);
	},
	finish: function()
	{
		GetFiles('Sequences');
		GetFiles('Music');
		GetFiles('Videos');
		GetFiles('Effects');
		GetFiles('Scripts');
	},
	allowExt:['ogg','fseq','eseq','mp4','sh']
});
</script> 
</div>
<div id='logViewer' title='Log Viewer' style="display: none">
  <pre>
  <div id='logText'>
  </div>
  </pre>
</div>
<?php	include 'common/footer.inc'; ?>
<script>
    $("#tabs").tabs({cache: true, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });
</script>
</body>
</html>
