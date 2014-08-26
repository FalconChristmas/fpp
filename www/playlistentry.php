<?php
class PlaylistEntry {
  public $type;
  public $songFile;
  public $seqFile;
  public $pause;
  public $eventName;
  public $eventID;
  public $index;
  public $pluginData;

  public function  __construct($type,$songFile,$seqFile,$pause,$eventName,$eventID,$pluginData,$index) {
    $this->type = $type;
    $this->songFile = $songFile;
    $this->seqFile = $seqFile;
    $this->pause = $pause;
    $this->eventName = $eventName;
    $this->eventID = $eventID;
    $this->index = $index;
    $this->pluginData = $pluginData;
  }
}
?>
