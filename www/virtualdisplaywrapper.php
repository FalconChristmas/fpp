<!DOCTYPE html>
<html lang="en">

<head>
  <?php
  include 'common/htmlMeta.inc';
  require_once('common.php');
  require_once('config.php');
  include_once 'common/menuHead.inc';

  if (isset($_GET['width'])) {
    $canvasWidth = (int) ($_GET['width']);
    $canvasHeight = (int) ($canvasWidth * 9.0 / 16.0);
  }

  ?>

  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
  <title>FPP Virtual Display</title>
</head>

<body>
  <div id="bodyWrapper">
    <?php
    $activeParentMenuItem = 'status';
    include 'menu.inc';
    ?>
    <div class="mainContainer">
      <h2 class="title">Virtual Display</h2>
      <div class="pageContent">
        <?
        require_once('virtualdisplaybody.php');
        ?>
        <br>NOTE: If the Virtual Display is not working, you need to add the HTTP Virtual Display Channel Output on the
        'Virtuals' tab of the Channel Outputs config page. Your pixel map is uploaded from xLights via FPP Connect by
        ticking the 'Models' checkbox for this device (it is saved as 'virtualdisplaymap' in FPP's configuration
        directory). The output's 'Preview' dropdown can select another map: the Default preview uses 'default.vdm'
        when present (otherwise 'virtualdisplaymap') and 'default-bg.jpg' when present (otherwise
        'virtualdisplaybackground.jpg'). A named preview such as 'christmas.vdm' uses that file from the configuration
        directory and 'christmas-bg.jpg' from the images directory when it exists.
      </div>
    </div>

    <?php include 'common/footer.inc'; ?>
  </div>
</body>

</html>