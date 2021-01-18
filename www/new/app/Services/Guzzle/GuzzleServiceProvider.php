<?php
declare(strict_types=1);

namespace App\Services\Guzzle;

use App\Kernel\Interfaces\ServiceProviderInterface;
use GuzzleHttp\Client;
use Pimple\Container;

class GuzzleServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'httpClient';
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
            return new Client();
        };
    }
}
