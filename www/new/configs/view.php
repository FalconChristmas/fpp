<?php
declare(strict_types=1);

return [

    'cache' => storage_path('cache/views'),
//    'cache' => false,
    'templates' => app_path('Views'),

    'debug' => env('APP_DEBUG', false),
    'auto_reload' => env('APP_DEBUG', false),
    'strict_variables' => env('APP_DEBUG', false),

];
