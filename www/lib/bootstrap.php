<?php

use FalconChristmas\Fpp\Container\Container;
use FalconChristmas\Fpp\Command\CommandExecutor;

spl_autoload_register(function ($class) {
    $prefix = 'FalconChristmas\\Fpp\\';
    $baseDir = __DIR__ . '/';

    if (!startsWith($class, $prefix)) {
        return;
    }

    $relativeClass = substr($class, strlen($prefix));
    $file = $baseDir . str_replace('\\', '/', $relativeClass) . '.php';

    if (file_exists($file)) {
        require $file;
    }
});

if (!function_exists('fpp')) {
    /**
     * Global helper to access the shared service container.
     *
     * @return \FalconChristmas\Fpp\Container\Container
     */
    function fpp()
    {
        static $container = null;

        if ($container === null) {
            $container = new Container();

            // Default service bindings; tests can override them as needed.
            $container->singleton('command', function () {
                return new CommandExecutor();
            });
        }

        return $container;
    }
}
