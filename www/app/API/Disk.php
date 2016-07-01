<?php
namespace FPP\API;


class Disk
{

    /**
     * Get disk usage info for root and media directories
     * @return array
     */
    public static function usage()
    {
        return [
            'root' => self::usageFor('/'),
            'media' => self::usageFor(fpp_media())
        ];
    }

    /**
     * Retrieve total and free space for a disk/directory
     * @param  string $directory
     * @return array
     */
    protected static function usageFor($directory)
    {
        $total = disk_total_space($directory);
        $free = disk_free_space($directory);
        $pct = $free * 100 / $total;
        return ['total' => get_size_symbol($total), 'free' => get_size_symbol($free), 'usage' => round($pct, 1).'%' ];
    }
}