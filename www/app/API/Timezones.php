<?php
namespace FPP\API;


class Timezones
{
    use UsesCli;

    public static function all()
    {
        $zones = collect(
            explode("\n", static::cli()->runAsUser("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort",
                function () {
                    throw new FPPSettingsException('Could not retrieve time zone listings');
                }
            )))->map(function ($zone) {
            return [urlencode($zone) => $zone];
        })->collapse();

        return $zones;
    }

    public static function current()
    {

    }
}
