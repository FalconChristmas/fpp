<?php
declare(strict_types=1);

use App\Kernel\App;

if (!function_exists('app')) {
    /**
     * Get App Instance
     *
     * @return App|null
     */
    function app(): ?App
    {
        return App::getInstance();
    }
}

if (!function_exists('container')) {
    /**
     * Get Container
     *
     * @param string|null $name
     * @return mixed|null
     */
    function container(string $name = null)
    {
        if ($name === null) {
            return app()->getContainer();
        }

        return app()->getContainer()[$name] ?? null;
    }
}

if (!function_exists('configs')) {
    /**
     * Get Configs
     *
     * @param string|null $name
     * @param mixed|null $default
     * @return mixed|null
     */
    function configs(string $name = null, $default = null)
    {
        $configs = app()->getConfigs() ?? null;

        if ($name === null) {
            return $configs;
        } else if ($configs === null) {
            return $default;
        }

        return $configs->get($name) ?? $default;
    }
}

if (!function_exists('env')) {
    /**
     * Gets the value of an environment variable
     *
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    function env(string $key, $default = null)
    {
        $value = getenv($key);

        if ($value === false) {
            return $default;
        }

        switch (strtolower($value)) {
            case 'true':
            case '(true)':
                return true;
            case 'false':
            case '(false)':
                return false;
            case 'empty':
            case '(empty)':
                return '';
            case 'null':
            case '(null)':
                return null;
        }

        $strLen = strlen($value);

        if ($strLen > 1 && $value[0] === '"' && $value[$strLen - 1] === '"') {
            return substr($value, 1, -1);
        }

        return $value;
    }
}

if (!function_exists('base_path')) {
    /**
     * Get the path to the base folder
     *
     * @param string $path
     * @return string
     */
    function base_path(string $path = ''): string
    {
        if ($path !== '' && $path[0] != '/') {
            $path = '/' . $path;
        }

        return realpath(__DIR__ . '/../../..') . $path;
    }
}

if (!function_exists('app_path')) {
    /**
     * Get the path to the application folder
     *
     * @param string $path
     * @return string
     */
    function app_path(string $path = ''): string
    {
        if ($path !== '' && $path[0] != '/') {
            $path = '/' . $path;
        }

        return base_path() . '/app' . $path;
    }
}

if (!function_exists('configs_path')) {
    /**
     * Get the path to the config folder
     *
     * @param string $path
     * @return string
     */
    function configs_path(string $path = ''): string
    {
        if ($path !== '' && $path[0] != '/') {
            $path = '/' . $path;
        }

        return base_path() . '/configs' . $path;
    }
}

if (!function_exists('public_path')) {
    /**
     * Get the path to the public folder
     *
     * @param string $path
     * @return string
     */
    function public_path(string $path = ''): string
    {
        if ($path !== '' && $path[0] != '/') {
            $path = '/' . $path;
        }

        return base_path() . '/public' . $path;
    }
}

if (!function_exists('storage_path')) {
    /**
     * Get the path to the storage folder
     *
     * @param string $path
     * @return string
     */
    function storage_path(string $path = ''): string
    {
        if ($path !== '' && $path[0] != '/') {
            $path = '/' . $path;
        }

        return base_path() . '/storage' . $path;
    }
}
