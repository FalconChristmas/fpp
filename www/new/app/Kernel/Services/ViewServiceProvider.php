<?php
declare(strict_types=1);

namespace App\Kernel\Services;

use App\Kernel\Interfaces\ServiceProviderInterface;
use Pimple\Container;
use Slim\Views\Twig;

class ViewServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'view';
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
            $configs = $container['configs']->get('view');

            return Twig::create($configs['templates'], $configs);
        };
    }
}
