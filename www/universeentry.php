<?php
class UniverseEntry
{
    public $active;
    public $universe;
    public $startAddress;
    public $size;
    public $type;
    public $unicastAddress;
    public $index;

    public function __construct($active, $universe, $startAddress, $size, $type, $unicastAddress, $index)
    {
        $this->active = $active;
        $this->universe = $universe;
        $this->startAddress = $startAddress;
        $this->size = $size;
        $this->type = $type;
        $this->unicastAddress = $unicastAddress;
        $this->index = $index;
    }
}
