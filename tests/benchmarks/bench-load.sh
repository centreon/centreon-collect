#!/bin/bash

for (( i=1 ; i<=15 ; i++ )) ; do
    (( nb_hosts=i*100 ))
    echo "i = $i"
    echo "nb_hosts = $nb_hosts"
    ./bench.py run load --test BENCH_LOAD_PASSIVE --var nb_hosts:$nb_hosts --label old-develop --allow-dirty
done
