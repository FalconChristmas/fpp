<?php
class PlaylistEntry {
  public $type;
  public $songFile;
  public $seqFile;
  public $pause;
  public $index;

  public function  __construct($type,$songFile,$seqFile,$pause,$index) {
    $this->type = $type;
    $this->songFile = $songFile;
    $this->seqFile = $seqFile;
    $this->pause = $pause;
    $this->index = $index;
  }
}
?>