<?php


namespace FPP\API;


class Git {
  use UsesCli;

  public static function branch()
  {
    $tag = static::cli()->run("git branch --list | grep '\\*' | awk '{print \$2}'");
    if (!$tag) {
        return 'Unknown';
    }
    return $tag;
  }

  public static function tags()
  {
    $tag = static::cli()->runAsUser('git describe --tags');
    if (!$tag) {
        return 'Unknown';
    }

    return $tag;
  }
}
