<?php

namespace FPP\API;

class Cpu {
  use UsesCli;

  public static function temperature()
  {
    $temp = self::cli()->runAsUser("cat /sys/class/thermal/thermal_zone*/temp");
    return ['c' => round($temp/1000, 1), 'f' => ((($temp/1000) * 1.8) + 32) ];
  }

  public static function usage()
  {
    return self::cli()->runAsUser("top -bn 1 | awk '{print $9}' | tail -n +8 | awk '{s+=$1} END {print s}'");
  }

}
