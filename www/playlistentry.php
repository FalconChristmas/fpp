<?php
class PlaylistEntry {
  public $type;
  public $songFile;
  public $seqFile;
  public $pause;
  public $videoFile;
  public $eventName;
  public $eventID;
  public $index;

  public function  __construct($type,$songFile,$seqFile,$pause,$videoFile,$eventName,$eventID,$index) {
    $this->type = $type;
    $this->songFile = $songFile;
    $this->seqFile = $seqFile;
    $this->pause = $pause;
    $this->videoFile = $videoFile;
    $this->eventName = $eventName;
    $this->eventID = $eventID;
    $this->index = $index;
  }
}
?>
