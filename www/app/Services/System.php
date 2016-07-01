<?php

namespace FPP\Services;

use Carbon\Carbon;
use FPP\Exceptions\FPPSettingsException;
use Illuminate\Support\Facades\Storage;
use Log;

class System
{
    public $cli;

    public function __construct(CommandLine $cli)
    {
        $this->cli = $cli;
    }

    /**
     * Finds and returns available network interfaces
     *
     * @return array
     */
    public function getNetworkInterfaces()
    {
        return collect(explode("\n", trim($this->cli->runAsUser("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0"))));
    }

    public function getInterfaceInfo($interface)
    {
        //compile and return network interface info
    }

    /**
     * Get DNS info
     *
     * @return array
     */
    public function getDNSInfo()
    {
        if (Storage::disk('pi')->exists('config/dns')) {
            $cfg = Storage::disk('pi')->get('config/dns');
            return parse_ini_string($cfg);
        }

        return [];
    }

    public function rebootPi()
    {

    }
    /**
     * Retrieve available soundcards
     *
     * @return array
     */
    public function getSoundCards()
    {
        $cardArr = [];

        $cards = explode("\n", $this->cli->run("for card in /proc/asound/card*/id; do echo -n \$card | sed 's/.*card\\([0-9]*\\).*/\\1:/g'; cat \$card; done",
            function ($errcode, $output) {
                Log::error('Error getting alsa cards for output!');
                throw new FPPSettingsException('Could not retrieve sound cards listing - ' . $output);
            }));

        foreach ($cards as $card) {
            $card = explode(':', $card);
            $cardArr[$card[0]] = $card[1];
        }

        return $cardArr;
    }

    /**
     * Get current timezone setting
     * @return string
     */
    public function getTimezone()
    {
        if (Storage::disk('pi')->exists('timezone')) {
            $timezone = trim(Storage::disk('pi')->get('timezone'));
            return $timezone;
        }
        return Carbon::now()->timezoneName;
    }

    /**
     * Get listing of all timezones
     *
     * @return mixed
     * @throws FPPSettingsException
     */
    public function getTimezones()
    {
        $zones = collect(
            explode("\n", $this->cli->runAsUser("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort",
                function () {
                    throw new FPPSettingsException('Could not retrieve time zone listings');
                }
            )))->map(function ($zone) {
            return [urlencode($zone) => $zone];
        })->collapse();

        return $zones;
    }

    /**
     * Get local time in friendly format
     * Format ex: Mon Feb 23 20:54:46 EST 2015
     *
     * @return string
     */
    public function getLocalTime()
    {
        return Carbon::now($this->getTimezone())->format('D M d H:i:s T Y');
    }

    /**
     * Checks if NTP is enabled
     * @return bool
     */
    public function ntpEnabled()
    {
        return (bool) $this->cli->runAsUser("ls -w 1 /etc/rc$(sudo runlevel | awk '{print $2}').d/ | grep ^S | grep -c ntp");
    }

    /**
     * Get current CPU usage of device
     * @return string
     */
    public function getCpuUsage()
    {
        return $this->cli->runAsUser("top -bn 1 | awk '{print $9}' | tail -n +8 | awk '{s+=$1} END {print s}'");
    }

    /**
     * Get current CPU temp
     * @return array
     */
    public function getTemperature()
    {
      $temp = $this->cli->runAsUser("cat /sys/class/thermal/thermal_zone*/temp");
      return ['c' => round($temp/1000, 1), 'f' => ((($temp/1000) * 1.8) + 32) ];
    }

    /**
     * Get current uptime
     * @return string
     */
    public function getUptime()
    {
      return $this->secondsToString($this->cli->runAsUser("awk '{print $1}' /proc/uptime"));
    }

    /**
     * Get current kernel version
     * @return string
     */
    public function getKernelVersion()
    {
      return $this->cli->runAsUser('/bin/uname -r') ?: 'Unknown';
    }

    /**
     * Get disk usage info for root and media directories
     * @return array
     */
    public function getDiskUsage()
    {
      return [
        'root' => $this->diskUsageFor('/'),
        'media' => $this->diskUsageFor(fpp_media())
      ];
    }

    /**
     * Helper function to convert seconds to human
     * readable format
     * @param  string $time
     * @return string
     */
    protected function secondsToString($time)
    {
      $seconds = $time%60;
      $mins = floor($time/60)%60;
      $hours = floor($time/60/60)%24;
      $days = floor($time/60/60/24);
      return $days > 0 ? $days . ' day'.($days > 1 ? 's' : '') : $hours.':'.$mins.':'.$seconds;
    }

    /**
     * Retrieve total and free space for a disk/directory
     * @param  string $directory
     * @return array
     */
    protected function diskUsageFor($directory)
    {
      $total = disk_total_space($directory);
      $free = disk_free_space($directory);
      $pct = $free * 100 / $total;
      return ['total' => get_size_symbol($total), 'free' => get_size_symbol($free), 'usage' => round($pct, 1).'%' ];
    }

}
