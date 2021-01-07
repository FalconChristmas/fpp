<?php
declare(strict_types=1);

return [

    // Logger //
    'name' => 'app',
    'maxFiles' => 7,
    'path' => env('LOG_TO_OUTPUT', false) === true ? 'php://stdout' : storage_path('logs/app.log'),

];
