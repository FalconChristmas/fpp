<?php
class PixelnetDMXentry {
  public $active;
  public $type;
  public $startAddress;

  public function  __construct($active,$type,$startAddress) {
    $this->active = $active;
    $this->type = $type;
    $this->startAddress = $startAddress;
  }
}
?>