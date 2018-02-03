<?php
class UniverseEntry {
  public $active;
  public $desc;
  public $universe;
  public $startAddress;
  public $size;
  public $type;
  public $unicastAddress;
  public $priority;
  public $index;

  public function  __construct($active,$desc,$universe,$startAddress,$size,$type,$unicastAddress,$priority,$index) {
    $this->active = $active;
	$this->desc = $desc;
    $this->universe = $universe;
    $this->startAddress = $startAddress;
    $this->size = $size;
    $this->type = $type;
    $this->unicastAddress = $unicastAddress;
    $this->priority = $priority;
    $this->index = $index;
  }
}
?>
