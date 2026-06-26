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
import subprocess
import sys
import xml.etree.ElementTree as ET


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


def generate_dryrun_xml(path):
    """Run `robot --dryrun` to expand templated (Examples:) tests and capture
    their resolved names, writing the result to `path`. Returns True on success,
    False if robot is unavailable or produced nothing usable.

    The exit code is ignored on purpose: suites whose keywords fail to resolve
    in dry-run still get their test structure written to output.xml, which is
    all we need here. No keyword is ever executed, so no daemon/DB is touched.
    """
    print(f"Running robot --dryrun to resolve templated test names "
          f"(cache: {path})...")
    try:
        subprocess.run(
            ['robot', '--dryrun', '--output', path,
             '--log', 'NONE', '--report', 'NONE', '.'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (FileNotFoundError, OSError):
        # 'robot' not on PATH (e.g. venv not activated): the caller emits a
        # single explanatory message and keeps the literal templated names.
        return False
    return os.path.exists(path) and os.path.getsize(path) > 0


def resolved_names_by_dir(xml_path):
    """Parse a dry-run output.xml and return {relative_dir: set(test_names)}
    using the resolved (variable-substituted) test names. Tests are attached to
    the directory of the .robot file suite that contains them, relative to the
    current directory so the keys match those produced by parse_dir()."""
    tests_root = os.path.abspath('.')
    result = {}

    def walk(suite):
        src = suite.get('source')
        tests = suite.findall('test')
        if tests and src:
            rel = os.path.relpath(os.path.dirname(os.path.abspath(src)),
                                  tests_root)
            result.setdefault(rel, set())
            for t in tests:
                result[rel].add(t.get('name'))
        for s in suite.findall('suite'):
            walk(s)

    root = ET.parse(xml_path).getroot()
    for s in root.findall('suite'):
        walk(s)
    return result


def _template_to_regex(name):
    """Turn a templated test name like 'Foo_${id}' into a compiled regex
    '^Foo_.+$' matching its resolved instances ('Foo_1', 'Foo_2', ...)."""
    parts = re.split(r'\$\{[^}]*\}', name)
    return re.compile('^' + '.+'.join(re.escape(p) for p in parts) + '$')


def reconcile_templates(flat, resolved, where):
    """Expand templated entries of a flat {test_name: doc} dict (parsed from
    .robot, where some names still contain ${...}) into their resolved instances
    from the dry-run, all sharing the template documentation. Non-templated
    entries are kept unchanged.

    robotframework-examples does not propagate [Documentation] to the expanded
    instances, hence the documentation is taken from the .robot template here.
    `where` is only used for warning messages."""
    new = {}
    # templates: list of (name, doc, regex, specificity). The specificity is the
    # number of literal (non-variable) characters: when several templates match
    # the same resolved name (e.g. 'BENCH_${n}_SERVICE...' and
    # 'BENCH_${n}_REVERSE_SERVICE...'), the most specific one wins so each
    # instance is attached to the right template.
    templates = []
    for name, doc in flat.items():
        if '${' in name:
            parts = re.split(r'\$\{[^}]*\}', name)
            templates.append((name, doc, _template_to_regex(name),
                              sum(len(p) for p in parts)))
        else:
            new[name] = doc
    matched = set()
    for r in sorted(resolved):
        if r in new:
            continue  # a regular (non-templated) test, already documented
        best = None
        for tpl in templates:
            if tpl[2].match(r) and (best is None or tpl[3] > best[3]):
                best = tpl
        if best is not None:
            new[r] = best[1]
            matched.add(best[0])
    for tpl in templates:
        if tpl[0] not in matched:
            # Keep the literal template name so the test is never dropped, even
            # if the dry-run could not expand it (e.g. import failure).
            new[tpl[0]] = tpl[1]
            print(f"Warning: no resolved instance found for templated test "
                  f"'{tpl[0]}' in {where}; keeping the literal name.")
    return new


dico = parse_dir('.')

# Expand templated (Examples:) test names using a dry-run, so that entries like
# Start_Stop_Engine_Broker_${id} become Start_Stop_Engine_Broker_1/2, etc.
#
# Lazy behaviour, like test-progress.py: the dry-run result is cached in
# DRYRUN_CACHE and reused as-is if it already exists. Pass -f/--force to
# regenerate it, or pass an explicit output.xml path as a positional argument to
# use that file instead. On any failure we degrade gracefully and keep the
# literal (templated) names.
DRYRUN_CACHE = '/tmp/dryrun-update-doc.xml'

_force = False
_explicit = None
for _a in sys.argv[1:]:
    if _a in ('-f', '--force'):
        _force = True
    else:
        _explicit = _a

if _explicit:
    _dryrun_xml = _explicit if os.path.exists(_explicit) else None
    if _dryrun_xml is None:
        print(f"Warning: dry-run file '{_explicit}' not found.")
else:
    _dryrun_xml = DRYRUN_CACHE
    if _force or not os.path.exists(_dryrun_xml) or \
            os.path.getsize(_dryrun_xml) == 0:
        if not generate_dryrun_xml(_dryrun_xml):
            _dryrun_xml = None
    else:
        print(f"Reusing existing dry-run cache '{_dryrun_xml}' "
              f"(use -f to regenerate).")

resolved = {}
if _dryrun_xml and os.path.exists(_dryrun_xml):
    try:
        resolved = resolved_names_by_dir(_dryrun_xml)
    except Exception as e:
        print(f"Warning: could not parse dry-run output ({e}); templated test "
              f"names will not be expanded.")

# Reconcile per-directory groups, then the root-level tests. If the dry-run
# produced no data at all (robot not found or dry-run failed), skip the whole
# reconciliation and keep the literal templated names, with a single message —
# rather than flooding one misleading warning per templated test.
if resolved:
    for _k in list(dico.keys()):
        if isinstance(dico[_k], dict):
            dico[_k] = reconcile_templates(dico[_k],
                                           resolved.get(_k[2:], set()), _k)
    _root_tests = {k: v for k, v in dico.items() if isinstance(v, str)}
    if _root_tests:
        _reconciled = reconcile_templates(_root_tests,
                                          resolved.get('.', set()), '.')
        for _k in _root_tests:
            del dico[_k]
        dico.update(_reconciled)
else:
    print("Warning: no dry-run data available (robot not found or dry-run "
          "failed); templated 'Examples:' test names are kept as-is. Run this "
          "script inside the robotframework venv to expand them.")

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
