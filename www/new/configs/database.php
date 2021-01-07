<?php
declare(strict_types=1);

return [

    // Database Support is Optional //
    // NOTE: Don't forget to load PDO Extensions //

    'type' => env('DB_CONNECTION', 'mysql'),

    'connections' => [

        'sqlite' => [
            'type' => 'sqlite',
            'database' => env('DB_DATABASE', storage_path('databases/app.sqlite'))
        ],

        'mysql' => [
            'type' => 'mysql',
            'host' => env('DB_HOST', 'localhost'),
            'port' => env('DB_PORT', '3306'),
            'database' => env('DB_DATABASE', 'app'),
            'username' => env('DB_USERNAME', 'app'),
            'password' => env('DB_PASSWORD', ''),
            'socket' => env('DB_SOCKET', null),
            'prefix' => '',
            'charset' => 'utf8mb4',
            'collation' => 'utf8mb4_unicode_ci',
            'options' => [
                PDO::ATTR_CASE => PDO::CASE_NATURAL,
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC,
            ],
        ],

        'pgsql' => [
            'type' => 'pgsql',
            'host' => env('DB_HOST', '127.0.0.1'),
            'port' => env('DB_PORT', '5432'),
            'database' => env('DB_DATABASE', 'app'),
            'username' => env('DB_USERNAME', 'app'),
            'password' => env('DB_PASSWORD', ''),
            'prefix' => '',
            'charset' => 'utf8',
            'option' => [
                PDO::ATTR_CASE => PDO::CASE_NATURAL
            ],
        ],

        'sqlsrv' => [
            'type' => 'mssql',
            'host' => env('DB_HOST', 'localhost'),
            'database' => env('DB_DATABASE', 'app'),
            'username' => env('DB_USERNAME', 'app'),
            'password' => env('DB_PASSWORD', ''),
        ],

    ],

];
