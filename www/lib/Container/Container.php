<?php

namespace FalconChristmas\Fpp\Container;

use FalconChristmas\Fpp\Command\CommandExecutor;
use FalconChristmas\Fpp\Container\Exception\ContainerException;
use FalconChristmas\Fpp\Container\Exception\NotFoundException;
use Throwable;

/**
 * Minimal IoC container to centralize service access inside the web UI.
 *
 * @property CommandExecutor $command
 */
class Container
{
    /**
     * @var array<string, mixed>
     */
    private $bindings = [];

    /**
     * @var array<string, bool>
     */
    private $singletons = [];

    /**
     * @var array<string, mixed>
     */
    private $instances = [];

    /**
     * Register a service factory or instance.
     *
     * @param string $key
     * @param mixed  $resolver
     */
    public function bind(string $key, mixed $resolver): void
    {
        $this->bindings[$key] = $resolver;
        unset($this->singletons[$key], $this->instances[$key]);
    }

    /**
     * Register a singleton factory or instance.
     *
     * @param string $key
     * @param mixed  $resolver
     */
    public function singleton(string $key, mixed $resolver): void
    {
        $this->bindings[$key] = $resolver;
        $this->singletons[$key] = true;
        unset($this->instances[$key]);
    }

    /**
     * Register an already instantiated singleton.
     *
     * @param string $key
     * @param mixed  $instance
     */
    public function instance(string $key, mixed $instance): void
    {
        $this->instances[$key] = $instance;
        $this->singletons[$key] = true;
        unset($this->bindings[$key]);
    }

    /**
     * Determine if a binding exists.
     *
     * @param string $key
     * @return bool
     */
    public function has(string $key): bool
    {
        return array_key_exists($key, $this->bindings) || array_key_exists($key, $this->instances);
    }

    /**
     * Resolve a binding or singleton.
     *
     * @param string $key
     * @return mixed
     * @throws NotFoundException
     * @throws ContainerException
     */
    public function make(string $key): mixed
    {
        if (array_key_exists($key, $this->instances)) {
            return $this->instances[$key];
        }

        if (!array_key_exists($key, $this->bindings)) {
            throw new NotFoundException("Service '{$key}' not registered in the container.");
        }

        $resolver = $this->bindings[$key];
        try {
            $instance = $this->resolve($resolver);
        } catch (Throwable $e) {
            throw new ContainerException("Error resolving service '{$key}'.", 0, $e);
        }

        if (isset($this->singletons[$key])) {
            $this->instances[$key] = $instance;
            unset($this->bindings[$key]);
        }

        return $instance;
    }

    /**
     * Remove a binding and its cached instance.
     *
     * @param string $key
     */
    public function forget(string $key): void
    {
        unset($this->bindings[$key], $this->instances[$key], $this->singletons[$key]);
    }

    /**
     * Remove all bindings, singletons, and cached instances.
     */
    public function clear(): void
    {
        $this->bindings = [];
        $this->singletons = [];
        $this->instances = [];
    }

    /**
     * Magic getter for syntactic sugar: $container->service
     *
     * @param string $key
     * @return mixed
     */
    public function __get(string $key): mixed
    {
        return $this->make($key);
    }

    /**
     * PSR-11 compatibility.
     *
     * @param string $id
     * @return mixed
     */
    public function get(string $id): mixed
    {
        return $this->make($id);
    }

    /**
     * Resolve the value for a binding.
     *
     * @param mixed $resolver
     * @return mixed
     */
    private function resolve(mixed $resolver): mixed
    {
        if (is_callable($resolver)) {
            return $resolver($this);
        }

        return $resolver;
    }
}
