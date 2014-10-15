<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
require_once('common/menuHead.inc');

function print_if_match($one, $two, $print)
{
  if ( $one == $two )
    return $print;
}
if ( isset($_POST['emailenable']) && !empty($_POST['emailenable']) )
{
  $emailenable = $_POST['emailenable'];
  WriteSettingToFile("emailenable",$emailenable);
   error_log("Set EmailEnable :" . $emailenable);
}

?>
<title><? echo $pageTitle; ?></title>
<script>
</script>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id="uiscripts" class="settings">
    <fieldset>
      <legend>Email Setup</legend>
      Outbound email requires a Gmail account to relay mail thru.  Nothing is stored in/on your
      Gmail account.  Gmail accounts are free by going to <a href="gmai.com" target="new">Gmail.com</a>.  By routing thru Gmail we go
      around when ISPs block outbound port 25 making you route email thru them.</br>
      All outbound email via scripts needs to be sent to <b>root@localhost</b>.
      <hr>
      <form name="email_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">
        <table id='email' cellspacing='2' border="1" width="100%">
          <tr>
            <td width="50%">Email Enable:</td>
            <td valign="middle">
              <?php PrintSettingCheckbox("Email: ", "emailenable", "1", "0"); ?>
          </tr>
          <tr>
            <td width="50%">Gmail Username:</td>
            <td><input name="emailguser" type="text" size="50" maxlength="30"></td>
          </tr>
          <tr>
            <td>Gmail Password:</td>
            <td><input name="emailgpass" type="password" size="50" maxlength="30"></td>
          </tr>
          <tr>
            <td>Destination From Text:</td>
            <td><input name="emailfromtext" type="text" size="50" maxlength="30"></td>
          </tr>
          <tr>
            <td>Destination From Email:</td>
            <td><input name="emailtoemail" type="text" size="50" maxlength="30"></td>
          </tr>
          <tr>
            <td colspan="2" align="center"><div id="submit">
                <input id="submit_button" name="submit_button" type="submit" class="buttons" value="Save">
              </div></td>
          </tr>
        </table>
      </form>
    </fieldset>
  </div>
</div>
<?php require_once 'common/footer.inc'; ?>
</body>
</html>
