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
        'Other' tab of the Channel Outputs config page. Your background image must be uploaded as a file called
        'virtualdisplaybackground.jpg' and your pixel map is uploaded from xLights via FPP Connect by ticking the
        'Models' checkbox for this device (it is saved as 'virtualdisplaymap' in FPP's configuration directory).
      </div>
    </div>

    <?php include 'common/footer.inc'; ?>
  </div>
</body>

</html>