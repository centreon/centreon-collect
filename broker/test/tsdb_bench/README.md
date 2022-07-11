# TSDB_BENCH

## Introduction
In order to choose the future time series database for centreon project, I developped this small software.
This software can be used in two ways, ingest data or select data

## How to compil it
First you need to have a linux computer, a C++17 compiler and a rust compiler. You need also conan to compile some needed librairies.

### compile quesdb client library
As you need the questdb client library, you have to clone it from https://github.com/questdb/c-questdb-client
Once you have cloned it:
* create a build directory: "build"
* cd build
* cmake ..
* make
### compile tsdb_bench project
Compilation needs the use of questdb client library. So you must add the questdb client library path in a variable environment.
In my case, I added export QUESTDB_ILP_DIR="/data/dev/tsdb/questdb/ilp_src/c-questdb-client" in my ~/.bashrc file.
Then, create a build directory in your tsdb_bench home, go into it and launch conan command:
```
conan install .. -s compiler.cppstd=17 -s compiler.libcxx=libstdc++11 --build=missing
```
then launch cmake
```
cmake -DCMAKE_BUILD_TYPE=Debug
```

## How to use it
As I said before, you have two modes: ingestion and select.

Example of ingestion usage:
```
bin/tsdb_bench --db-conf ../questdb/questdb.json --metric-nb 10000000 --nb-host 100 
--nb-service-by-host 10 --metric-id-nb 10000 --bulk-size 1000000 --time-frame 366
```
Example of select usage:
```
build/bin/tsdb_bench --db-conf influxdb/victoria.json --select-begin "2021-07-01 00:00:00" --select-end "2022-07-01 00:00:00" --nb-point 1000
```
You can also use select_bench_victoria.sh script to do statisticals select benchs