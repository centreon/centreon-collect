#!/usr/bin/python3
#
# Copyright 2023-2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.
import os
import re


def complete_doc(dico, ff):
    f = open(ff, 'r')
    content = f.readlines()
    f.close()
    r = re.compile(r"\s+\[Documentation]\s+(\S.*)$")
    rd = re.compile(r"\s+\.\.\.    \s*(.*)$")

    in_test = False
    in_documentation = False
    test_name = ""
    for line in content:
        if in_documentation:
            m = rd.match(line)
            if m:
                dico[test_name] += " " + m.group(1)
                continue
            else:
                test_name = ""
                in_documentation = False

        if in_test:
            if line.startswith("***"):
                break
            if len(test_name) != 0 and "[Documentation]" in line:
                m = r.match(line)
                if m:
                    in_documentation = True
                    dico[test_name] = m.group(1)
            if not line.startswith('\t') and not line.startswith("  "):
                test_name = line.strip()
        elif line.startswith("*** Test Cases ***"):
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

On AlmaLinux, the following commands should work to initialize your robot tests:

```bash
dnf install "Development Tools" python3-devel -y

pip3 install -U robotframework \\
        robotframework-databaselibrary \\
        robotframework-examples pymysql \\
        robotframework-requests psutil \\
        robotframework-httpctrl boto3 \\
        GitPython unqlite py-cpuinfo


pip3 install grpcio grpcio_tools

#you need also to provide opentelemetry proto files at the project root with this command
git clone https://github.com/open-telemetry/opentelemetry-proto.git opentelemetry-proto

#Then you must have something like that:
#root directory/bbdo
#              /broker
#              /engine
#              /opentelemetry-proto
#              /tests
```

We need some perl modules to run the tests, you can install them with the following command:

```bash
dnf install perl-HTTP-Daemon-SSL
dnf install perl-JSON
```

To work with gRPC, we also need to install some python modules.

On rpm based system, we have to install:
```
yum install python3-devel -y
```

On deb based system, we have to install:
```
apt-get install python3-dev
```

And then we can install the required python modules:
```
pip3 install grpcio grpcio_tools
```

Now it should be possible to initialize the tests with the following commands:

```bash
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
        out.write(f"{idx}. **{k}**: {dico[k]}\n")
        idx += 1
        count += 1
    else:
        tests = list(dico[k].keys())
        tests.sort()
        idx = 1
        for kk in tests:
            if isinstance(dico[k][kk], str):
                out.write(f"{idx}. **{kk}**: {dico[k][kk]}\n")
                idx += 1
                count += 1
            else:
                print("This tree is too deep")
                exit(1)
        out.write("\n")

out.close()
print(f"{count} tests are documented now.")
