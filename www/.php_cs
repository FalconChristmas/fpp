<?php

return PhpCsFixer\Config::create()
    ->setRules(array(
        '@PSR2' => true,
    ))
    ->setFinder(
        PhpCsFixer\Finder::create()
            ->exclude('tests/Fixtures')
            ->in(__DIR__)
    )
;
