<?php
require_once('auth.php');

require_once("config.php");

$thisdir = dirname(__FILE__);

if ( !empty($_POST) && $_POST["password"] == "disabled" )
{
  unlink("$mediaDirectory/uipasswd");
}

if ( isset($_POST['password1']) && isset($_POST['password2']))
{
  if (($_POST['password1'] != "") && ($_POST['password1'] == $_POST['password2']))
  {
    // true - hash the password & save it
    file_put_contents("$mediaDirectory/uipasswd", password_hash($_POST['password1'], PASSWORD_DEFAULT));
  }
}

function checked_if_equal($value1, $value2)
{
  if ( $value1 == $value2 )
  {
    echo "checked=\"checked\"";
  }
}

function hide_if_equal($value1, $value2)
{
  if ( $value1 == $value2 )
  {
    echo "style=\"display: none\"";
  }
}

$pw = file_exists("$mediaDirectory/uipasswd");

?>

<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/validate.min.js"></script>
<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id="uipassword" class="settings">
    <fieldset>
      <legend>UI Password</legend>
      <form name="password_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">
              <label for="password_enabled">Enabled:</label>
              <input type="radio" name="password" id="password_enabled" value="enabled" <?php checked_if_equal($pw, true); ?>></input>
              <label for="password_disabled">Disabled:</label>
              <input type="radio" name="password" id="password_disabled" value="disabled" <?php checked_if_equal($pw, false); ?>></input>

    <div id="password" <?php hide_if_equal($pw, false); ?>>
      <table width= "100%" border="0" cellpadding="2" cellspacing="2">
        <tr>
          <td>Username:</td>
          <td>admin</td>
        </tr><tr>
          <td><label for="password1">Password:</label></td>
          <td><input name="password1" id="password1" type="password" size="40" maxlength="40"></td>
        </tr><tr>
          <td><label for="password2">Confirm Password:</label></td>
          <td><input name="password2" id="password2" type="password" size="40" maxlength="40"></td>
        </tr>
      </table>
    </div>

<div id="errors">
</div>

<div id="submit">
<input id="submit_button" name="submit_button" type="submit" class="buttons" value="Submit">
</div>
      </form>
    </fieldset>
  </div>
</div>
<?php  include 'common/footer.inc'; ?>

<script>


$(document).ready(function(){
  $("input[name$='password']").change(function() {
    if ( $(this).val() == "enabled" )
      $("#password").show();
    else
      $("#password").hide();
  });
});



var validator = new FormValidator('password_form', [{
    name: 'password1',
    rules: 'required'
}, {
    name: 'password2',
    display: 'password confirmation',
    rules: 'required|matches[password1]|min_length[8]'
}], function(errors, evt) {

    // Don't validate when we're disabling password protection
    if ( $('input[name=password]:checked', '#uipassword').val() == "disabled" ) {
        evt.submit();
    }

    var selector_errors = $('#errors');

    if (errors.length > 0) {
        selector_errors.empty();

        for (var i = 0, errorLength = errors.length; i < errorLength; i++) {
            selector_errors.append(errors[i].message + '<br />');
        }

        selector_errors.fadeIn(200);

        if (evt && evt.preventDefault) {
            evt.preventDefault();
        } else if (event) {
            event.returnValue = false;
        }
    }

});
</script>


</body>
</html>
