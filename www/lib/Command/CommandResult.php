<?php

namespace FalconChristmas\Fpp\Command;

class CommandResult
{
    private string $command;
    private array $output;
    private int $exitCode;

    /**
     * @param string $command
     * @param array<int, string> $output
     * @param int $exitCode
     */
    public function __construct(string $command, array $output, int $exitCode)
    {
        $this->command = $command;
        $this->output = $output;
        $this->exitCode = $exitCode;
    }

    /**
     * @return string
     */
    public function getCommand(): string
    {
        return $this->command;
    }

    /**
     * @return array<int, string>
     */
    public function getOutput(): array
    {
        return $this->output;
    }

    /**
     * @return int
     */
    public function getExitCode(): int
    {
        return $this->exitCode;
    }

    /**
     * @return bool
     */
    public function wasSuccessful(): bool
    {
        return $this->exitCode === 0;
    }

    /**
     * @return string
     */
    public function __toString(): string
    {
        return implode("\n", $this->output);
    }
}
