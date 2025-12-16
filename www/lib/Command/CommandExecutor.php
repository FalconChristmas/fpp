<?php

namespace FalconChristmas\Fpp\Command;

/**
 * Thin wrapper around exec() to keep shell interactions centralized/testable.
 */
class CommandExecutor
{
    /**
     * @param string $command
     * @param bool $redirectStdErr
     * @return CommandResult
     */
    public function run(string $command, bool $redirectStdErr = true)
    {
        if ($redirectStdErr) {
            $command .= ' 2>&1';
        }

        $output = [];
        $exitCode = 0;
        exec($command, $output, $exitCode);

        return new CommandResult($command, $output, $exitCode);
    }
}
