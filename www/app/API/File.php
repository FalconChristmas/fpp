<?php

namespace FPP\API;

use FPP\Filesystem\FileAccessor;

/**
 * Manipulating files on the local filesystem
 */
class File
{
    public static function disk($disk = null)
    {
        $disk = (is_null($disk)) ? 'local' : $disk;

        return new FileAccessor($disk, app('filesystem')->disk($disk));
    }

    public static function __callStatic($method, $args)
    {
        return call_user_func_array([self::disk(), $method], $args);
    }
}
