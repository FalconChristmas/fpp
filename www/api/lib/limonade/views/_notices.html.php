<?php if(!empty($notices)): ?>
<div class="lim-debug lim-notices">
  <h4> &#x2192; Notices and warnings</h4>
  <dl>
  <?php $cpt = 1; foreach($notices as $notice): ?>
    <dt>[<?php echo $cpt.'. '.error_type($notice['errno'])?>]</dt>
    <dd>
    <?php echo $notice['errstr']?> in <strong><code><?php echo $notice['errfile']?></code></strong> 
    line <strong><code><?php echo $notice['errline']?></code></strong>
    </dd>
  <?php $cpt++; endforeach; ?>
  </dl>
  <hr>
</div>
<?php endif; ?>