<?php
declare(strict_types=1);

namespace App\Services\Slugify;

use App\Kernel\Interfaces\ServiceProviderInterface;
use Cocur\Slugify\Slugify;
use Pimple\Container;

class SlugifyServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'slugifier';
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
            return new Slugify();
        };
    }
}
