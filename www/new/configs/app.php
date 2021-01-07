<?php
declare(strict_types=1);

return [

    // Application Details //
    'env' => env('APP_ENV', null),

    'timezone' => 'UTC',


    // App Debugging //
    'debug' => env('APP_DEBUG', false),
    'displayErrorDetails' => env('APP_DEBUG', false),

    'logErrors' => env('LOG_ERRORS', true),
    'logErrorDetails' => env('LOG_ERRORS_DETAILS', true),
    'logToOutput' => env('LOG_TO_OUTPUT', false),

];
