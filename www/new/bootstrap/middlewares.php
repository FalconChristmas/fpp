<?php
declare(strict_types=1);

use App\Kernel\Middlewares\SessionMiddleware;
use App\Kernel\Middlewares\ViewMiddleware;

return [

    SessionMiddleware::class,

    ViewMiddleware::class,

];
