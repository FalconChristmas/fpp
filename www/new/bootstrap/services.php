<?php
declare(strict_types=1);

use App\Kernel\Services\Configs\ConfigsServiceProvider;
use App\Kernel\Services\FlashServiceProvider;
use App\Kernel\Services\Logger\LoggerServiceProvider;
use App\Kernel\Services\ViewServiceProvider;

return [

    ConfigsServiceProvider::class,

    LoggerServiceProvider::class,

    FlashServiceProvider::class,

    ViewServiceProvider::class,

];
