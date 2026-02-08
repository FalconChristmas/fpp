<?php

namespace FalconChristmas\Fpp\Shell;

/**
 * Thin wrapper around exec() to keep shell interactions centralized/testable.
 */
class ShellExecutor
{
    /**
     * @param string $command
     * @param bool $redirectStdErr
     * @return ShellResult
     */
    public function run(string $command, bool $redirectStdErr = true): ShellResult
    {
        if ($redirectStdErr) {
            $command .= ' 2>&1';
        }

        $output = [];
        $exitCode = 0;
        exec($command, $output, $exitCode);

        return new ShellResult($command, $output, $exitCode);
    }
}
