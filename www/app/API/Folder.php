<?php

namespace FPP\API;

use FPP\Filesystem\FolderAccessor;

/**
 * Manipulating folders on the local filesystem
 */
class Folder
{
    public static function disk($disk = null)
    {
        $disk = (is_null($disk)) ? 'local' : $disk;

        return new FolderAccessor($disk, app('filesystem')->disk($disk));
    }

    public static function __callStatic($method, $args)
    {
        return call_user_func_array([self::disk(), $method], $args);
    }
}
