<?php
declare(strict_types=1);

use App\Services\Guzzle\GuzzleServiceProvider;
use App\Services\Slugify\SlugifyServiceProvider;

return [

    'slugifier' => SlugifyServiceProvider::class,
    'httpClient' => GuzzleServiceProvider::class,

];
