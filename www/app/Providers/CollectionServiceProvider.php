<?php

namespace FPP\Providers;

use Illuminate\Support\Collection;
use Illuminate\Support\ServiceProvider;

class CollectionServiceProvider extends ServiceProvider
{
    /**
     * Bootstrap the application services.
     *
     * @return void
     */
    public function boot()
    {
        /**
         * Returns all odd number keys
         */
        Collection::macro('odd', function () {
          return $this->values()->filter(function ($value, $i) {
            return $i % 2 !== 0;
          });
        });

        /**
         * Returns all even number keys
         */
        Collection::macro('even', function () {
          return $this->values()->filter(function ($value, $i) {
            return $i % 2 == 0;
          });
        });

        /**
         * Run callback if collection is empty
         */
        Collection::macro('ifEmpty', function ($callback) {
          if ($this->isEmpty()) {
            $callback();
          }
          return $this;
        });
    }

    /**
     * Register the application services.
     *
     * @return void
     */
    public function register()
    {
        //
    }
}
