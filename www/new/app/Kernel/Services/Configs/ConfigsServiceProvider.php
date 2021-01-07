<?php
declare(strict_types=1);

namespace App\Kernel\Services\Configs;

use App\Kernel\Interfaces\ServiceProviderInterface;
use Pimple\Container;

class ConfigsServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'configs';
    }

    /**
     * Register new service on dependency container
     *
     * @param Container $container
     * @return mixed
     */
    public function register(Container $container)
    {
        return new Configs($container);
    }
}
