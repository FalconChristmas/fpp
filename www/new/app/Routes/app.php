<?php
declare(strict_types=1);

use App\Controllers\HomeController;
use App\Controllers\DeviceinformationController;
use Slim\App;

/**
 * @var $app App
 */

$app->get('/', [(new HomeController()), 'index']);
$app->get('/device/information', [(new DeviceinformationController()), 'index']);
