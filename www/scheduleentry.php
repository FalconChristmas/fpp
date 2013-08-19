<?php
class ScheduleEntry {
  public $enable;
  public $playlist;
  public $startDay;
  public $startHour;
  public $startMinute;
  public $startSecond;
  public $endDay;
  public $endHour;
  public $endMinute;
  public $endSecond;
  public $repeat;

  public function  __construct($enable,$playlist,$startDay,$startHour,$startMinute,$startSecond,
	                             $endDay,$endHour,$endMinute,$endSecond,$repeat) {
    $this->enable = $enable;
    $this->playlist = $playlist;
    $this->startDay = $startDay;
    $this->startHour = $startHour;
    $this->startMinute = $startMinute;
    $this->startSecond = $startSecond;
    $this->endHour = $endHour;
    $this->endMinute = $endMinute;
    $this->endSecond = $endSecond;
    $this->repeat = $repeat;
  }
}
?>