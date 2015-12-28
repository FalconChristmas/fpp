<?php

/**
 * Returns the path to the fpp directory
 */
if ( ! function_exists('fpp_dir')) {
    function fpp_dir()
    {
        return dirname(base_path());
    }
}


if ( ! function_exists('fpp_media')) {
    function fpp_media($directory = false)
    {
        $conf = config('fpp.media');


        if ($directory) {
            $sub = config('fpp.' . $directory);
            $conf = $conf.$sub;
        }

        return $conf;
    }
}

if ( ! function_exists('fpp_config')) {
    function fpp_config($config)
    {

        if (env('APP_DEV')) {
            return storage_path().getenv('FPP_'.strtoupper($config));
        }

        return config('fpp.'.$config);
    }
}


if ( ! function_exists('set_active')) {

    function set_active($path, $active = 'active')
    {
        return Request::is($path) ? $active : '';
    }
}


/**
 * Appends a trailing slash.
 *
 * Will remove trailing forward and backslashes if it exists already before adding
 * a trailing forward slash. This prevents double slashing a string or path.
 *
 * The primary use of this is for paths and thus should be used for paths. It is
 * not restricted to paths and offers no specific path support.
 *
 *
 * @param string $string What to add the trailing slash to.
 * @return string String with trailing slash added.
 */
if ( ! function_exists('trailingslashit')) {
    function trailingslashit($string)
    {
        return untrailingslashit($string) . '/';
    }
}

/**
 * Removes trailing forward slashes and backslashes if they exist.
 *
 * The primary use of this is for paths and thus should be used for paths. It is
 * not restricted to paths and offers no specific path support.
 *
 *
 * @param string $string What to remove the trailing slashes from.
 * @return string String without the trailing slashes.
 */
if ( ! function_exists('untrailingslashit')) {

    function untrailingslashit($string)
    {
        return rtrim($string, '/\\');
    }
}
