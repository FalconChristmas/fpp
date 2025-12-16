<?php

declare(strict_types=1);

namespace Tests;

use FalconChristmas\Fpp\Shell\ShellResult;
use PHPUnit\Framework\TestCase;

final class CronjobsTest extends TestCase
{
    private object $fake;
    private static string $originalIncludePath;

    public static function setUpBeforeClass(): void
    {
        self::$originalIncludePath = get_include_path();
        set_include_path(__DIR__ . '/../stubs' . PATH_SEPARATOR . self::$originalIncludePath);

        // cronjobs.php outputs HTML; buffer it while loading the Crontab class.
        ob_start(function () {
            return '';
        });
        require_once __DIR__ . '/../../cronjobs.php';
        ob_end_clean();
    }

    public static function tearDownAfterClass(): void
    {
        set_include_path(self::$originalIncludePath);
    }

    protected function setUp(): void
    {
        $this->fake = new class {
            public string $lastCommand = '';
            public array $nextOutput = [];
            public int $exitCode = 0;

            public function run(string $command, bool $redirectStdErr = true): ShellResult
            {
                $this->lastCommand = $command;

                return new ShellResult($command, $this->nextOutput, $this->exitCode);
            }
        };

        // Reset and swap the command executor.
        $container = fpp();
        $container->clear();
        $container->instance('shell', $this->fake);
    }

    public function testGetJobsRunsCrontabList(): void
    {
        $this->fake->nextOutput = ["job1\r\njob2\r\n"];

        $jobs = \Crontab::getJobs();

        $this->assertSame('crontab -l', $this->fake->lastCommand);
        $this->assertSame(['job1', 'job2'], array_values($jobs));
    }

    public function testSaveJobsRunsCrontabWrite(): void
    {
        $jobs = ['* * * * * /bin/true', '0 1 * * * /usr/bin/backup'];

        \Crontab::saveJobs($jobs);

        $expected = 'echo "* * * * * /bin/true' . "\r\n" . '0 1 * * * /usr/bin/backup" | crontab -';
        $this->assertSame($expected, $this->fake->lastCommand);
    }
}
