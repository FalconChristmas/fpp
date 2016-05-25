<?php

namespace FPP\Services;

class Git
{
    public $cli;
    /**
     *
     *  @param CommandLine $cli [description]
     */
    public function __construct(CommandLine $cli)
    {
        $this->cli = $cli;
    }
    /**
     * Retrieve current git branch
     *
     * @return string
     */
    public function getGitBranch()
    {
        $tag = $this->cli->run("git branch --list | grep '\\*' | awk '{print \$2}'");
        if (!$tag) {
            return 'Unknown';
        }

        return $tag;
    }
    /**
     * Retrieve tag of currently active commit
     *
     * @return string
     */
    public function getGitTag()
    {
        $tag = $this->cli->runAsUser('git describe --tags');
        if (!$tag) {
            return 'Unknown';
        }

        return $tag;
    }

}
