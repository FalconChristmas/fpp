<?php

declare(strict_types=1);

namespace Tests;

use FalconChristmas\Fpp\Container\Container;
use FalconChristmas\Fpp\Container\Exception\ContainerException;
use FalconChristmas\Fpp\Container\Exception\NotFoundException;
use PHPUnit\Framework\TestCase;
use stdClass;

final class ContainerTest extends TestCase
{
    public function testBindResolvesCallableEachTime(): void
    {
        $container = new Container();
        $callCount = 0;

        $container->bind(
            'counter',
            function () use (&$callCount) {
                $callCount++;
                return $callCount;
            }
        );

        $this->assertSame(1, $container->make('counter'));
        $this->assertSame(2, $container->make('counter'));
        $this->assertTrue($container->has('counter'));
    }

    public function testSingletonCachesResolvedInstance(): void
    {
        $container = new Container();

        $container->singleton('object', function () {
            return new stdClass();
        });

        $first = $container->make('object');
        $second = $container->make('object');

        $this->assertSame($first, $second);
        $this->assertTrue($container->has('object'));
    }

    public function testInstanceRegistersPreBuiltSingleton(): void
    {
        $container = new Container();
        $instance = new stdClass();
        $instance->value = 'preset';

        $container->instance('preset', $instance);

        $this->assertTrue($container->has('preset'));
        $this->assertSame($instance, $container->make('preset'));
    }

    public function testForgetRemovesBindingsAndInstances(): void
    {
        $container = new Container();

        $container->singleton('transient', function () {
            return new stdClass();
        });

        $container->make('transient');
        $this->assertTrue($container->has('transient'));

        $container->forget('transient');
        $this->assertFalse($container->has('transient'));

        $this->expectException(NotFoundException::class);
        $container->make('transient');
    }

    public function testMagicGetDelegatesToMake(): void
    {
        $container = new Container();
        $container->bind('string', function () {
            return 'value';
        });

        $this->assertSame('value', $container->string);
    }

    public function testMakeThrowsForUnknownBinding(): void
    {
        $container = new Container();

        $this->expectException(NotFoundException::class);
        $this->expectExceptionMessage("Service 'missing' not registered in the container.");

        $container->make('missing');
    }

    public function testCallableExceptionsAreWrappedInContainerException(): void
    {
        $container = new Container();
        $container->bind('broken', function () {
            throw new \LogicException('broken');
        });

        $this->expectException(ContainerException::class);
        $this->expectExceptionMessage("Error resolving service 'broken'.");

        $container->make('broken');
    }

    public function testClearRemovesAllEntries(): void
    {
        $container = new Container();
        $container->singleton('one', function () {
            return new stdClass();
        });
        $container->bind('two', function () {
            return new stdClass();
        });
        $container->instance('three', new stdClass());

        $container->make('one');
        $container->make('two');

        $container->clear();

        $this->assertFalse($container->has('one'));
        $this->assertFalse($container->has('two'));
        $this->assertFalse($container->has('three'));

        $this->expectException(NotFoundException::class);
        $container->make('one');
    }
}
