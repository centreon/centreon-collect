#!/bin/bash
#set -x
avg=(0 0 0 0 0 0 0 0 0 0 0 0 0)
nb_try=10
for try_index in $(seq 1 ${nb_try})
do
    start='2021-07-01'
    avg_index=0
    for end in 2021-08-01 2021-09-01 2021-10-01 2021-11-01 2021-12-01 2022-01-01 2022-02-01 2022-03-01 2022-04-01 2022-05-01 2022-06-01 2022-07-01
    do
        duration=`build/bin/tsdb_bench --db-conf warp10/warp10.json --select-begin "${start} 00:00:00" --select-end "${end} 00:00:00" --nb-point 1000 2>>year_bench_select_warp10.log`
        start=${end}
        avg[avg_index]=`expr ${avg[avg_index]} + ${duration}`
        avg_index=$((avg_index + 1))
    done
done
#year
avg_year=0
for try_index in $(seq 1 ${nb_try})
do
    duration=`build/bin/tsdb_bench --db-conf warp10/warp10.json --select-begin "2021-07-01 00:00:00" --select-end "2022-07-01 00:00:00" --nb-point 1000 2>>year_bench_select_warp10.log`
    avg_year=`expr ${avg_year} + ${duration}`

done


for avg_index in $(seq 0 11)
do
    echo `expr ${avg[avg_index]} / ${nb_try}`
done
echo `expr ${avg_year} / ${nb_try}`


