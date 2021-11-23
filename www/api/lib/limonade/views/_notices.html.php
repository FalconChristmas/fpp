<?php if(!empty($notices)): ?>
<div class="lim-debug lim-notices">
  <h4> &#x2192; Notices and warnings</h4>
  <dl>
  <?php $cpt = 1; foreach($notices as $notice): ?>
    <dt>[<?= $cpt.'. '.error_type($notice['errno'])?>]</dt>
    <dd>
    <?= $notice['errstr']?> in <strong><code><?= $notice['errfile']?></code></strong> 
    line <strong><code><?= $notice['errline']?></code></strong>
    </dd>
  <?php $cpt++; endforeach; ?>
  </dl>
  <hr>
</div>
<?php endif; ?>