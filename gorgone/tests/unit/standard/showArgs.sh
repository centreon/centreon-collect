#!/bin/sh
i=1
for arg in "$@"; do
    echo -n " $i:$arg"
    i=$((i+1))
done
