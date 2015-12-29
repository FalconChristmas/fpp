<?php

namespace FPP\Facades;

use Illuminate\Support\Facades\Facade;

class Plugins extends Facade
{
    
    protected static function getFacadeAccessor()
    {
        return 'plugins';
    }
}
