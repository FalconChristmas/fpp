<?php
declare(strict_types=1);

namespace App\Kernel\Services;

use App\Kernel\Interfaces\ServiceProviderInterface;
use Pimple\Container;
use Slim\Flash\Messages;

class FlashServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'flash';
    }

    /**
     * Register new service on dependency container
     *
     * @param Container $container
     * @return mixed
     */
    public function register(Container $container)
    {
        return function () {
            return new Messages;
        };
    }
}
