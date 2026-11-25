*** Settings ***
Documentation       Benchmarks: they measure, they do not assert, and they take minutes to tens
...                 of minutes each. The tags below reach every test of this directory, so that
...                 `robot -e unstable .` from tests/ never runs one of them, whatever a suite
...                 forgot to write. `./bench.py run` and `robot benchmarks/<file>.robot` are
...                 not affected: they name the file and do not exclude anything.

Test Tags           bench    unstable
