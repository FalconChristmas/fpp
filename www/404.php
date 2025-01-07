<!DOCTYPE html>
<html lang="en">

<head>
  <?php include 'common/htmlMeta.inc';
  require_once('common.php');
  include 'common/menuHead.inc'; ?>
  <title><? echo $pageTitle; ?></title>
</head>

<body>
  <div id="bodyWrapper">
    <?php include 'menu.inc'; ?>
    <br />

    <div id='rebootFlag' style='display:block;'>404 - PAGE NOT FOUND</div>

    <?php include 'common/footer.inc'; ?>
  </div>
</body>

</html>