<?php
declare(strict_types=1);

use App\Handlers\ErrorHandler;
use App\Handlers\ShutdownHandler;

return [

    // Handlers //
    'errorHandler' => ErrorHandler::class,

    'shutdownHandler' => ShutdownHandler::class,

];
