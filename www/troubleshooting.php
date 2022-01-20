<!DOCTYPE html>
<html>
<?php
require_once('troubleshootingCommands.php');
?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
<style>
.back2top{
        position: fixed;
        bottom: 10px;
        right: 20px;
        z-index: 99;
        background: red;
        color: #fff;
        border-radius: 30px;
        padding: 15px;
        font-weight: bold;
        line-height: normal;
        border: none;
        opacity: 0.5;
    }
</style>
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
          <a class="troubleshoot-anchor" name="<? echo $header ?>">.</a><h3><? echo $title . ':&nbsp;&nbsp;&nbsp;&nbsp;' . $command; ?></h3>
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
<button type="button" class="back2top">Back to top</button>

<script type="application/javascript">

/*
 * Anchors are dynamiclly via ajax thus auto scrolling if anchor is in url
 * will fail.  This will workaround that problem by forcing a scroll 
 * afterward dynamic content is loaded.
 */
function fixScroll() {
    // Remove the # from the hash, as different browsers may or may not include it
    var hash = location.hash.replace('#','');

    if(hash != ''){
        var elements = document.getElementsByName(hash);
        if (elements.length > 0) {
            elements[0].scrollIntoView();
        }
   }
}


$( document ).ready(function() {

  document.querySelector("#troubleshooting-hot-links").innerHTML = '<?php echo $hotLinks ?>';
  $(".back2top").click(() => $("html, body").animate({scrollTop: 0}, "slow") && false);
  $(window).scroll(function(){
      if ($(this).back2top() > 100) $('.scrollToTop').fadeIn();
      else $('.back2top').fadeOut();
  });

<?
  foreach($jsArray as $key => $command)
{
  $url = "./troubleshootingHelper.php?key=" . urlencode($command);
?>
      $.ajax({
            url: "<?php echo $url ?>",
            type: 'GET',
            success: function(data) {
//                data = "<button type=\"button\" class=\"back2top\">Back to top</button>" + data;
                document.querySelector("#<?php echo $key ?>").innerHTML = data;
                fixScroll();
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
