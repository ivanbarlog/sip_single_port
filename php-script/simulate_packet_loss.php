<?php

if (PHP_SAPI !== 'cli') {
    echo "script must be run from PHP CLI\n";
}

$path = sprintf("%s%s..%s%s", dirname(__FILE__), DIRECTORY_SEPARATOR, DIRECTORY_SEPARATOR, "packet_loss");

echo (int)file_exists($path);
