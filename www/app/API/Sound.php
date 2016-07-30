<?php
namespace FPP\API;

use FPP\Exceptions\FPPSettingsException;
use Log;

class Sound
{
    use UsesCli;

    public static function cards()
    {
        $cards = collect(explode("\n", static::cli()->run("for card in /proc/asound/card*/id; do echo -n \$card | sed 's/.*card\\([0-9]*\\).*/\\1:/g'; cat \$card; done",
            function ($errcode, $output) {
                Log::error('Error getting alsa cards for output!');
                throw new FPPSettingsException('Could not retrieve sound cards listing - ' . $output);
            })))->reduce(function ($result, $card) {
            $card = explode(':', $card);
            return $result[$card[0]] = $card[1];
        });

        return $cards;
    }

}
