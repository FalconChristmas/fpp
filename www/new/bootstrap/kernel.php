<?php
declare(strict_types=1);

use Dotenv\Dotenv;

if (file_exists(base_path('.env'))) {
    Dotenv::createImmutable(base_path())
        ->load();
}
