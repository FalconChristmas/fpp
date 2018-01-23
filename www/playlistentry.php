<?php
class PlaylistEntry {
  public $type;
  public $songFile;
  public $seqFile;
  public $pause;
  public $scriptName;
  public $eventName;
  public $eventID;
  public $index;
  public $pluginData;
  public $entry;

  public function  __construct($type,$songFile,$seqFile,$pause,$scriptName,$eventName,$eventID,$pluginData,$entry,$index) {
    $this->type = $type;
    $this->songFile = $songFile;
    $this->seqFile = $seqFile;
    $this->pause = $pause;
    $this->scriptName = $scriptName;
    $this->eventName = $eventName;
    $this->eventID = $eventID;
    $this->index = $index;
    $this->pluginData = $pluginData;
	$this->entry = $entry;
  }
}
?>
