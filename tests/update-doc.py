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
    with open(ff, 'r') as f:
        content = f.readlines()
    r = re.compile(r"\s+\[Documentation]\s+(\S.*)$")
    rd = re.compile(r"\s+\.\.\.(\s*)(.*)$")

    in_test = False
    in_documentation = False
    gherkin = False
    test_name = ""
    for line in content:
        if in_documentation:
            m = rd.match(line)
            if m:
                if gherkin:
                    txt = m.group(2)
                    txt = txt.strip()
                    nl = ''
                    if len(txt) > 0:
                        if txt.upper().startswith("WHEN "):
                            txt = re.sub(r"When", "* **WHEN**", txt, flags=re.IGNORECASE, count=1)
                            #txt = txt.replace("When", "* **When**", 1)
                        if txt.upper().startswith("GIVEN "):
                            txt = re.sub(r"Given", "* **GIVEN**", txt, flags=re.IGNORECASE, count=1)
                            #txt = txt.replace("Given", "* **Given**", 1)
                            nl = '\n'
                        if txt.upper().startswith("THEN "):
                            txt = re.sub(r"Then", "* **THEN**", txt, flags=re.IGNORECASE, count=1)
                            #txt = txt.replace("Then", "* **Then**", 1)
                        if txt.upper().startswith("AND WHEN "):
                            txt = re.sub(r"And when", "* **AND WHEN**", txt, flags=re.IGNORECASE, count=1)
                            #txt = txt.replace("And", "* **And**", 1)
                        if txt.upper().startswith("AND "):
                            txt = re.sub(r"And", "* **AND**", txt, flags=re.IGNORECASE, count=1)
                            #txt = txt.replace("And", "* **And**", 1)
                        if txt.upper().startswith("SCENARIO: "):
                            txt = re.sub(r"Scenario:", "**SCENARIO:**", txt, flags=re.IGNORECASE, count=1)
                            nl = '\n'
                            #txt = txt.replace("Scenario:", "**Scenario:**", 1)
                        if txt.upper().startswith("BACKGROUND:"):
                            txt = re.sub(r"Background:", "**BACKGROUND:**", txt, flags=re.IGNORECASE, count=1)
                            nl = '\n'
                        dico[test_name] += f"{nl}\n     {txt}"
                else:
                    dico[test_name] += " " + m.group(2)
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
                    if m.group(1).upper().startswith("GIVEN") or m.group(1).upper().startswith("WHEN"):
                        gherkin = True
                        dico[test_name] = "\n     * " + re.sub(r"(Given|When)", lambda m: f"**{m.group(1).upper()}**", m.group(1), flags=re.IGNORECASE)
                    elif m.group(1).upper().startswith("SCENARIO:") or m.group(1).upper().startswith("FEATURE:"):
                        gherkin = True
                        txt = re.sub(r"(Scenario:|Feature:)", lambda m: f"**{m.group(1).upper()}**", m.group(1), flags=re.IGNORECASE)
                        dico[test_name] = txt
                    else:
                        gherkin = False
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

From a Centreon host, you need to install Robot Framework.

On AlmaLinux, we have to install some python packages, some perl packages:

```bash
dnf install "Development Tools" python3-devel -y
dnf install perl-HTTP-Daemon-SSL -y
dnf install perl-JSON -y
```

On rpm based system, we have to execute the following commands (maybe to update a little):

```bash
yum install "Development Tools" python3-devel -y
yum install perl-HTTP-Daemon-SSL -y
yum install perl-JSON -y
```

On deb based system, we have to execute:


```bash
apt-get install python3-dev openssh-server
```

Once these packages, we recommand to create a python virtual environment to play with robot framework.

You can do that as you prefer, here we use uv. The first step is to install it:

```bash
curl -LsSf https://astral.sh/uv/install.sh | less
```

Once installed, you have to create a virtual environment, we create it in the centreon-collect/tests directory:

```bash
cd centreon-collect/tests
uv venv --python=python3.11 robotframework
```

And now, we can install the required python modules for our tests:

```bash
uv pip install -U robotframework \\
        robotframework-databaselibrary \\
        robotframework-examples pymysql \\
        robotframework-requests psutil \\
        robotframework-httpctrl boto3 \\
        GitPython unqlite py-cpuinfo pyjwt \\
        grpcio grpcio_tools
```

When you want to enable the virtual environment, you just have to execute the following command:

```bash
cd centreon-collect/tests
source robotframework/bin/activate
```

Now it should be possible to initialize several files to execute the tests with the following commands:

```bash
./init-proto.sh
./init-sql.sh
```

Then to run tests, you can use the following commands

```
robot -e unstable .
```

And it is also possible to execute a specific test, for example:

```
robot broker/sql.robot
```

In order to execute bench tests (broker-engine/bench.robot), you need also to
install py-cpuinfo, cython, unqlite and boto3

```bash
uv pip install py-cpuinfo cython unqlite gitpython boto3
```

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

out.write(f"\n{count} tests currently implemented.\n")
out.close()
print(f"{count} tests are documented now.")
