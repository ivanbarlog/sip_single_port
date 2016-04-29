<?php

const ASSOCIATIVE = true;

if (PHP_SAPI !== 'cli') {
    printf("0\n");
    die("script must be run from PHP CLI\n");
}

$json = $argv[1];

if (empty($json)) {
    printf("0\n");
    die("you need to provide JSON string as an argument.\n");
}

$body = json_decode($json, ASSOCIATIVE);

if (count($body) !== 4) {
    printf("0\n");
    die("invalid body\n");
}

printf("1\n");

foreach ($body as $item) {
    printf("%s\n", $item);
}
