<?php

if (PHP_SAPI !== 'cli') {
        die("script must be run from PHP CLI\n");
}

$ip = $argv[1];
$port = $argv[2];

if (empty($ip) || empty($port)) {
	die("both arguments must be filled in");
}

printf("%s\n%s\n", $ip, $port);
