<?php
namespace FPP\API;


trait UsesCli
{
    public static function cli()
    {
        return new Cli();
    }

}