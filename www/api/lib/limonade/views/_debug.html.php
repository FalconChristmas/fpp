<?php if(option('env') > ENV_PRODUCTION && option('debug')): ?>
  <?php if(!$is_http_error): ?>
  <p>[<?php echo error_type($errno)?>]
  	<?php echo $errstr?> (in <strong><?php echo $errfile?></strong> line <strong><?php echo $errline?></strong>)
  	</p>
  	<?php endif; ?>


  <?php if($debug_args = set('_lim_err_debug_args')): ?>
  <h2 id="debug-arguments">Debug arguments</h2>
  	<pre><code><?php echo h(print_r($debug_args, true))?></code></pre>
  <?php endif; ?>
  
  <h2 id="limonade-options">Options</strong></h2>
  <pre><code><?php echo h(print_r(option(), true))?></code></pre>
  <p class="bt top"><a href="#header">[ &#x2191; ]</a></p>
  
  <h2 id="environment">Environment</h2>
  <pre><code><?php echo h(print_r(env(), true))?></code></pre>
  <p class="bt top"><a href="#header">[ &#x2191; ]</a></p>
  
  <h2 id="debug-backtrace">Backtrace</h2>
  <pre><code><?php echo h(print_r(debug_backtrace(), true))?></code></pre>
  <p class="bt top"><a href="#header">[ &#x2191; ]</a></p>

  <div id="debug-menu">
    
    <?php if($debug_args = set('_lim_err_debug_args')): ?>
    <a href="#debug-arguments">Debug arguments</a> |
    <?php endif; ?>
    <a href="#limonade-options">Options</a> |
    <a href="#environment">Environment</a> |
    <a href="#debug-backtrace">Backtrace</a> |
    <a href="#header">[ &#x2191; ]</a>
  </div>
  
<?php endif; ?>