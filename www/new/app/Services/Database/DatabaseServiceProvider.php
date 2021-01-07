<?php
declare(strict_types=1);

namespace App\Services\Database;

use App\Kernel\Interfaces\ServiceProviderInterface;
use Pimple\Container;

class DatabaseServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'db';
    }

    /**
     * Register new service on dependency container
     *
     * @param Container $container
     * @return mixed
     */
    public function register(Container $container)
    {
        return function (Container $container) {
            return (new Database())->builder($container);
        };
    }
}
