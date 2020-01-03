<!DOCTYPE html>
<html>
<?php
require_once('config.php');
?>
<head>
<?php	include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Files</title>

<script>
    $(function() {
    $('#tblDocs').on('mousedown', 'tr', function(event,ui){
          $('#tblDocs tr').removeClass('selectedentry');
          $(this).addClass('selectedentry');
          DocNameSelected  = $(this).find('td:first').text();
		  SetButtonState('#btnDownloadDoc','enable');
		  SetButtonState('#btnViewDoc','enable');
    });
  });

	$(document).ready(function() {
		GetFiles("Docs");
	});

</script>
<style>
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

<body>
<div id="bodyWrapper">
<?php	include 'menu.inc'; ?>
<div id="docViewer">
  <br />
      <div id= "divDoc">
        <fieldset class="fs">
          <legend> FPP Documentation </legend>
          <div id="divDocData">
            <table id="tblDocs">
            </table>
          </div>
          <hr />
          <div class='right'>
            <input onclick="ViewFile('Docs', DocNameSelected);" id="btnViewDoc" class="disableButtons" type="button"  value="View" />
            <input onclick= "DownloadFile('Docs', DocNameSelected);" id="btnDownloadDoc" class="disableButtons" type="button"  value="Download" />
          </div>
        </fieldset>
      </div>
</div>
<div id='fileViewer' title='File Viewer' style="display: none">
  <div id='fileText'>
  </div>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
