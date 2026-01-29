<?php

declare(strict_types=1);

namespace Tests;

use FalconChristmas\Fpp\Shell\ShellExecutor;
use FalconChristmas\Fpp\Shell\ShellResult;
use PHPUnit\Framework\TestCase;

final class ShellExecutorTest extends TestCase
{
    public function testRunReturnsSuccessfulResult(): void
    {
        $executor = new ShellExecutor();
        $result = $executor->run('echo success');

        $this->assertInstanceOf(ShellResult::class, $result);
        $this->assertSame('echo success 2>&1', $result->getCommand());
        $this->assertSame(['success'], $result->getOutput());
        $this->assertSame(0, $result->getExitCode());
        $this->assertTrue($result->wasSuccessful());
        $this->assertSame("success", (string) $result);
    }

    public function testRunWithoutRedirectStdErr(): void
    {
        $executor = new ShellExecutor();
        $result = $executor->run('echo success', false);

        $this->assertInstanceOf(ShellResult::class, $result);
        $this->assertSame('echo success', $result->getCommand());
        $this->assertSame(['success'], $result->getOutput());
        $this->assertSame(0, $result->getExitCode());
        $this->assertTrue($result->wasSuccessful());
        $this->assertSame("success", (string) $result);
    }

    public function testRunCapturesFailureExitCode(): void
    {
        $executor = new ShellExecutor();
        $result = $executor->run('php -r "exit(3);"');

        $this->assertSame(3, $result->getExitCode());
        $this->assertFalse($result->wasSuccessful());
        $this->assertSame([], $result->getOutput());
        $this->assertSame('', (string) $result);
    }
}
