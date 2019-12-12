<?php
class ScheduleEntry {
  public $enable;
  public $playlist;
  public $startDay;
  public $startHour;
  public $startMinute;
  public $startSecond;
  public $endHour;
  public $endMinute;
  public $endSecond;
  public $repeat;
  public $startDate;
  public $endDate;
  public $hardStop;

  public function  __construct($enable,$playlist,$startDay,$startHour,$startMinute,$startSecond,
	                             $endHour,$endMinute,$endSecond,$repeat,$startDate,$endDate,$hardStop) {
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
    $this->startDate = $startDate;
    $this->endDate = $endDate;
    $this->hardStop = $hardStop;
  }
}
?>
