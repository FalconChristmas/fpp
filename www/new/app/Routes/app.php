<?php
declare(strict_types=1);

use App\Controllers\HomeController;
use Slim\App;

/**
 * @var $app App
 */

$app->get('/', [(new HomeController()), 'index']);
