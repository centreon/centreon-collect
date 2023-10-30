#!/usr/bin/python3
import os
import re


def complete_doc(dico, ff):
    f = open(ff, 'r')
    content = f.readlines()
    f.close()
    r = re.compile(r"\s+\[Documentation]\s+(\S.*)$")

    in_test = False
    test_name = ""
    for l in content:
        if in_test:
            if l.startswith("***"):
                break
            if len(test_name) != 0 and "[Documentation]" in l:
                m = r.match(l)
                if m:
                    dico[test_name] = m.group(1)
                    test_name = ""
            if not l.startswith('\t') and not l.startswith("  "):
                test_name = l.strip()
        elif l.startswith("*** Test Cases ***"):
            in_test = True


def parse_dir(d):
    r = re.compile(r".*\.robot$")
    retval = {}
    content = os.listdir(d)
    for f in content:
        ff = d + '/' + f
        if os.path.isdir(ff):
            ret = parse_dir(ff)
            if len(ret) > 0:
                retval[ff] = ret
        if r.match(ff) and os.path.isfile(ff):
            complete_doc(retval, ff)
    return retval


dico = parse_dir('.')

out = open('README.md', 'w')
out.write("""# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8 and
robot files are formatted using `robottidy --overwrite tests`.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework

On CentOS 7, the following commands should work to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary robotframework-examples pymysql

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

./init-proto.sh
./init-sql.sh
```

On other rpm based distributions, you can try the following commands to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary robotframework-httpctrl pymysql

yum install python3-devel -y

pip3 install grpcio grpcio_tools

./init-proto.sh
./init-sql.sh
```

Then to run tests, you can use the following commands

```
robot .
```

And it is also possible to execute a specific test, for example:

```
robot broker/sql.robot
```
In order to execute bench tests (broker-engine/bench.robot), you need also to
install py-cpuinfo, cython, unqlite and boto3

pip3 install py-cpuinfo cython unqlite gitpython boto3

## Implemented tests

Here is the list of the currently implemented tests:

""")

keys = list(dico.keys())
keys.sort()
count = 0

idx = 1
for k in keys:
    name = k[2:]
    name = name.replace('-', '/')
    name = name.replace('_', ' ').capitalize()
    out.write(f"### {name}\n")
    if isinstance(dico[k], str):
        out.write(f"{idx}. [x] **{k}**: {dico[k]}\n")
        idx += 1
        count += 1
    else:
        tests = list(dico[k].keys())
        tests.sort()
        idx = 1
        for kk in tests:
            if isinstance(dico[k][kk], str):
                out.write(f"{idx}. [x] **{kk}**: {dico[k][kk]}\n")
                idx += 1
                count += 1
            else:
                print("This tree is too deep")
                exit(1)
        out.write("\n")

out.close()
print(f"{count} tests are documented now.")
