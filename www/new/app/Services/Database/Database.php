<?php
declare(strict_types=1);

namespace App\Services\Database;

use App\Kernel\Exceptions\ConfigsException;
use Medoo\Medoo;
use Pimple\Container;

class Database
{

    /**
     * Builder Logger
     *
     * @param Container $container
     * @return Medoo
     * @throws ConfigsException
     */
    public function builder(Container $container): Medoo
    {
        $configs = $container['configs']->get('database') ?? null;
        $type = $configs['type'] ?? null;

        $configs = $configs['connections'][$type] ?? null;

        if ($configs === null || $type === null) {
            throw new ConfigsException('Check you database configurations');
        }

        return new Medoo($this->convertToMedoo($configs));
    }

    /**
     * Convert Configs to Medoo Format
     *
     * @param array $configs
     * @return array
     */
    protected function convertToMedoo(array $configs): array
    {
        $convert = [
            'type' => 'database_type',
            'host' => 'server',
            'database' => 'database_name',
            'options' => 'option',
        ];

        array_walk($configs, function ($value, $key) use ($convert, &$configs) {
            $newkey = array_key_exists($key, $convert) ? $convert[$key] : false;

            if ($newkey !== false) {
                $configs[$newkey] = $value;
                unset($configs[$key]);
            }
        });

        return $configs;
    }
}
