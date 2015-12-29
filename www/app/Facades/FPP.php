<?php


namespace FPP\Facades;

use Illuminate\Support\Facades\Facade;

class FPP extends Facade {
    protected static function getFacadeAccessor() { return 'fpp'; }
}
