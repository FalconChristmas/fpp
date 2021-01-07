<?php
declare(strict_types=1);

namespace App\Kernel\Services\Configs;

use App\Kernel\Interfaces\ConfigsInterface;
use DirectoryIterator;
use Pimple\Container;

class Configs implements ConfigsInterface
{

    /**
     * Container
     *
     * @var Container
     */
    protected $container;

    /**
     * Configuration items
     *
     * @var array
     */
    protected $items = [];


    /**
     * Configuration constructor
     *
     * @param Container $container
     */
    public function __construct(Container $container)
    {
        $this->container = $container;

        $this->loadConfigurationFiles();

        date_default_timezone_set($this->get('app.timezone'));

        mb_internal_encoding('UTF-8');
    }

    /**
     * Determine if the given configuration value exists.
     *
     * @param string $key
     * @return bool
     */
    public function has(string $key): bool
    {
        return array_has($this->items, $key);
    }

    /**
     * Get the specified configuration value.
     *
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    public function get(string $key = null, $default = null)
    {
        return array_get($this->items, $key, $default);
    }

    /**
     * Set a given configuration value.
     *
     * @param array|string $key
     * @param mixed $value
     */
    public function set($key, $value = null): void
    {
        if (is_array($key)) {
            foreach ($key as $innerKey => $innerValue) {
                array_set($this->items, $innerKey, $innerValue);
            }
        } else {
            array_set($this->items, $key, $value);
        }
    }


    /**
     * Load the configuration items from files
     */
    protected function loadConfigurationFiles(): void
    {
        foreach ($this->getConfigurationFiles() as $key => $path) {
            $key = current(explode('|', $key));

            $getConfig = $this->get($key);

            if ($getConfig !== null) {
                $this->set($key, array_replace_recursive(require $path, $getConfig));
            } else {
                $this->set($key, require $path);
            }
        }
    }

    /**
     * Get configuration files
     *
     * @return array
     */
    protected function getConfigurationFiles(): array
    {
        $files = [];

        $app = require configs_path('app.php');
        $env = $app['env'] ?? 'prod';

        $configPath = configs_path();
        $paths = [$configPath];

        if ($env !== null) {
            $envPath = $configPath . '/' . $env;

            if (file_exists($envPath)) {
                array_unshift($paths, $envPath);
            }
        }

        foreach ($paths as $path) {
            foreach (new DirectoryIterator($path) as $file) {
                // Exclude files started by _ //
                if ($file->isDir() || $file->getExtension() !== 'php' || $file->getFilename()[0] === '_') {
                    continue;
                }

                $prefix = (strpos($file->getRealPath(), '/' . $env . '/') !== false) ? '|' . $env : '';

                $files[basename($file->getRealPath(), '.php') . $prefix] = $file->getRealPath();
            }
        }

        return $files;
    }
}
