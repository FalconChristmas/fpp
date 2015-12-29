<?php
namespace FPP\Facades;


use Illuminate\Support\Facades\Facade;

class Pi extends Facade {
    protected static function getFacadeAccessor() { return 'pi'; }
}