<?php
require_once('config.php');
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php	include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>File Uploads</title>
<script type="text/javascript" src="js/fpp.js"></script>
<script src="js/jquery-migrate-1.1.1.min.js"></script>
<script src="js/jquery-1.9.1.js"></script>
<script src="js/jquery-ui.js"></script>
<script src="js/upload/jquery.js" type="text/javascript"></script>
<script src="js/upload/ajaxupload.js" type="text/javascript"></script>
<link rel="stylesheet" href="css/fpp.css" />
<link rel="stylesheet" href="css/classicTheme/style.css" type="text/css" media="all" />
<script>
    $(function() {
		$('#tblSequences').on('mousedown', 'tr', function(event,ui){
					$('#tblSequences tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
					SequenceNameSelected  = $(this).find('td.seqName').text();
					var i=0;

		});

		$('#tblMusic').on('mousedown', 'tr', function(event,ui){
					$('#tblMusic tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
					SongNameSelected  = $(this).find('td.songName').text();
					var i=0;

		});

	});
</script>
<style>
#top {
	min-width: 768px;
	width: 768px;
	margin-left: auto;
	margin-right: auto;
}

#tblSequences {
	width: 100%;
}
#tblMusic {
	width: 100%;
}
.seqDetails {
	font-size: 13px;
}
.songDetails {
	font-size: 13px;
}
.seqName {
	width: 65%;
}
.seqTime {
	width: 35%;
}
.songTime {
	width: 35%;
}
.songName {
	width: 65%;
}

#uploader_div {
	display: block;
	min-height: 300px;
	width: 100%;
	clear: both;
	padding-top: 10px;
}
fieldset {
	height: 100%;
	min-height: 270px;
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

<body onload="GetSequenceFiles(); GetMusicFiles();">
<div id="bodyWrapper">
<?php	include 'menu.inc'; ?>
<div id="top">
  <br />
  <div id= "divSeq">
    <fieldset class="fs">
      <legend> Sequence &amp; Effects Files (.fseq, .eseq) </legend>
      <div id="divSeqData">
        <table id="tblSequences">
        </table>
      </div>
      <hr />
      <div class='right'>
        <input onclick="DeleteSequence();" name="btnDeleteSequence" class="buttons" type="button"  value="Delete" />
      </div>
    </fieldset>
  </div>
  <div id= "divMusic">
    <fieldset  class="fs">
      <legend> Music Files (.ogg) </legend>
      <div id="divMusicData">
        <table id="tblMusic">
        </table>
      </div>
      <hr />
      <div class='right'>
        <input onclick= "DeleteMusic();" name="btnDeleteMusic" class="buttons" type="button"  value="Delete" />
      </div>
    </fieldset>
  </div>
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
		GetSequenceFiles();
		GetMusicFiles();
	},
	allowExt:['ogg','fseq']
});
</script> 
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</div>
</html>
