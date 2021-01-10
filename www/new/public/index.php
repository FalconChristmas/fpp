<?php
declare(strict_types=1);

// To help the built-in PHP dev server, check if the request was actually for something which should probably be served as a static file
if ((PHP_SAPI === 'cli-server') && is_file(__DIR__ . parse_url($_SERVER['REQUEST_URI'])['path'])) {
    return false;
}

require __DIR__ . '/../vendor/autoload.php';

require __DIR__ . '/../bootstrap/app.php';
