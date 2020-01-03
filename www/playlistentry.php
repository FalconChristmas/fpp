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

	//Special Processing for events
      if (strtolower($this->type) == "event") {
          //if any remove leading 0's in event majorid and minorid
          //casts to integer to when playlist saved, numbers are not strings
          //but keep data in entry
          $tmp_major_id = intval($this->entry->majorID);
          $tmp_minor_id = intval($this->entry->minorID);
          //only do this if value is less than 10 to save something weird happening
            //majorid
          if ($tmp_major_id < 10) {
              $this->entry->majorID = intval(ltrim($tmp_major_id, 0));
          }else{
              $this->entry->majorID = intval($tmp_major_id);
          }
            //minorid
          if ($tmp_minor_id < 10) {
              $this->entry->minorID = intval(ltrim($tmp_minor_id, 0));
          }else{
              $this->entry->minorID = intval($tmp_minor_id);
          }
      }
  }

}
?>
