#!/bin/sh
# print $2 (UTF-8) to stdout encoded in $1 (UTF-8, ISO-8859-1...) to simulate
# plugins whose output is not UTF-8
printf '%s' "$2" | iconv -f UTF-8 -t "$1"
