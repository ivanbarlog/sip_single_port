<?php

if (PHP_SAPI !== 'cli') {
	die("script must be run from PHP CLI\n");
}

// $argv[0] holds script name
$f_ip = $argv[1];
$f_port = $argv[2];
$t_ip = $argv[3];
$t_port = $argv[4];

if (empty($f_ip) || empty($f_port) || empty($t_ip) || empty($t_port)) {
	die("all 4 arguments must be filled in");
}

$body = array(
	'f_ip' => $f_ip,
	'f_port' => $f_port,
	't_ip' => $t_ip,
	't_port' => $t_port,
);

// echo JSON string
printf("%s\n", json_encode($body));
