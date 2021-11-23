  <h1><?= h(error_http_status($errno));?></h1>
  <?php if($is_http_error): ?>
  <p><?= h($errstr)?></p>
  <?php endif; ?>
  
  <?=  render('_debug.html.php', null, $vars); ?>