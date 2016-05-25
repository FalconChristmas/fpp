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


if ( ! function_exists('fpp_version')) {

    function fpp_version()
    {

        $fpp_version = "v" . exec("git --git-dir=".dirname(base_path())."/.git/ describe --tags", $output, $return_val);
        if ( $return_val != 0 ) {
            $fpp_version = "Unknown";
        }

        $git_branch = exec("git --git-dir=".dirname(base_path())."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
        if ( $return_val != 0 ) {
            $git_branch = "Unknown";
        }

        if (!preg_match("/^$git_branch(-.*)?$/", $fpp_version)) {
            $fpp_version .= " ($git_branch branch)";
        }

        return $fpp_version;
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


/**
 * Get the user
 */
function user()
{
    if (! isset($_SERVER['SUDO_USER'])) {
        return $_SERVER['USER'];
    }
    return $_SERVER['SUDO_USER'];
}
