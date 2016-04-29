<?php

if (PHP_SAPI !== 'cli') {
    die("script must be run from PHP CLI\n");
}

$path = sprintf("%s%s..%s%s", dirname(__FILE__), DIRECTORY_SEPARATOR, DIRECTORY_SEPARATOR, "notify");

unlink($path);
