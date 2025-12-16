<?php

declare(strict_types=1);

namespace Tests;

use FalconChristmas\Fpp\Command\CommandExecutor;
use FalconChristmas\Fpp\Command\CommandResult;
use PHPUnit\Framework\TestCase;

final class CommandExecutorTest extends TestCase
{
    public function testRunReturnsSuccessfulResult(): void
    {
        $executor = new CommandExecutor();
        $result = $executor->run('echo success');

        $this->assertInstanceOf(CommandResult::class, $result);
        $this->assertSame('echo success 2>&1', $result->getCommand());
        $this->assertSame(['success'], $result->getOutput());
        $this->assertSame(0, $result->getExitCode());
        $this->assertTrue($result->wasSuccessful());
        $this->assertSame("success", (string) $result);
    }

    public function testRunWithoutRedirectStdErr(): void
    {
        $executor = new CommandExecutor();
        $result = $executor->run('echo success', false);

        $this->assertInstanceOf(CommandResult::class, $result);
        $this->assertSame('echo success', $result->getCommand());
        $this->assertSame(['success'], $result->getOutput());
        $this->assertSame(0, $result->getExitCode());
        $this->assertTrue($result->wasSuccessful());
        $this->assertSame("success", (string) $result);
    }

    public function testRunCapturesFailureExitCode(): void
    {
        $executor = new CommandExecutor();
        $result = $executor->run('php -r "exit(3);"');

        $this->assertSame(3, $result->getExitCode());
        $this->assertFalse($result->wasSuccessful());
        $this->assertSame([], $result->getOutput());
        $this->assertSame('', (string) $result);
    }
}
