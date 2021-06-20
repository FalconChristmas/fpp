<!DOCTYPE html>
<html>
<?php
require_once('troubleshootingCommands.php');
?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
</head>

<body>
<div id="bodyWrapper">
  <?php 
  $activeParentMenuItem = 'help'; 
  include 'menu.inc'; ?>
  <div class="mainContainer">

    <h1 class="title">Troubleshooting</h1>
    <div class="pageContent">
      <h2>Troubleshooting Commands</h2>
        <div id="troubleshooting-hot-links"> </div>
        <hr>
        <div style="overflow: hidden; padding: 10px;">
      <div class="clear"></div>
  <?
  
  $hotLinks = "<ul>";
  $jsArray = array();
  $cnt = 1;

  foreach ($commands as $title => $command)
  {
    $key = "command_" . $cnt;
    $header = "header_" . $cnt;
    $hotLinks .= "<li><a href=\"#$header\">$title</a></li>";
    $jsArray[$key] = $title;
  ?>
          <a name="<? echo $header ?>"><h3><? echo $title . ':&nbsp;&nbsp;&nbsp;&nbsp;' . $command; ?></h3></a>
          <pre id="<? echo $key ?>"><i>Loading...</i></pre>
          <hr>
  <?
    ++$cnt;
  }
  $hotLinks .="</ul>";
  ?>

    </div>
    
  </div>
  <?php include 'common/footer.inc'; ?>
</div>

<script type="application/javascript">

$( document ).ready(function() {

  document.querySelector("#troubleshooting-hot-links").innerHTML = '<?php echo $hotLinks ?>';

<?
  foreach($jsArray as $key => $command)
{
  $url = "./troubleshootingHelper.php?key=" . urlencode($command);
?>
      $.ajax({
            url: "<?php echo $url ?>",
            type: 'GET',
            success: function(data) {
                document.querySelector("#<?php echo $key ?>").innerHTML = data;
            },
            error: function() {
                DialogError('Failed to query comand', "Error: Unable to query for <?php echo $command ?>");
            }
        });

<?php
}
?>

}); // end document ready
</script>

</body>
</html>
