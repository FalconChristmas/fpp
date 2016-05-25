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
}
