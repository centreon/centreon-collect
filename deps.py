#!/usr/bin/python3
"""
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
"""

import argparse
import re
import os
import json
import sys

ESC = '\x1b'
YELLOW = ESC + '[1;33m'
GREEN = ESC + '[1;32m'
ORANGE = ESC + '[1;31m'
RESET = ESC + '[0m'


def parse_command(entry):
    """
    Returns all the include directories used in the compilation command and also
    a file to get all the precompiled files. They are stored in a dictionary,
    the first one with key 'include_dirs' and the second one with key 'pch'.

    Args:
        entry: An entry of the compile_commands.json

    Returns: A list of the directories with absolute paths.
    """
    command = entry['command']
    args = command.split(' ')

    retval = {}

    if "clang" in args[0]:
        for a in args:
            if 'cmake_pch.hxx.pch' in a:
                retval['pch'] = a[:4]
                break
    elif "g++" in args[0]:
        for a in args:
            if 'cmake_pch.hxx' in a:
                retval['pch'] = a
                break

    # -I at first
    retval['include_dirs'] = [a[2:].strip()
                              for a in args if a.startswith('-I')]

    # -isystem at second
    try:
        idx = args.index("-isystem")
        retval['include_dirs'].append(args[idx + 1])
    except ValueError:
        pass

    # and system headers at last
    retval['include_dirs'].append("/usr/include")
    if os.path.exists("/usr/include/c++/12"):
        retval['include_dirs'].append("/usr/include/c++/12")
    elif os.path.exists("/usr/include/c++/10"):
        retval['include_dirs'].append("/usr/include/c++/10")
    return retval


def get_headers(full_name):
    headers = []
    with open(full_name, 'r') as f:
        lines = f.readlines()

    r_include = re.compile(r"^\s*#include\s*[\"<](.*)[\">]")
    for line in lines:
        m = r_include.match(line)
        if m:
            headers.append(m.group(1))
    return headers


def build_full_headers(includes, headers):
    retval = []
    for header in headers:
        for inc in includes:
            file = f"{inc}/{header}"
            if os.path.isfile(file):
                retval.append((file, header))
                break
    return retval


def get_precomp_headers(precomp):
    try:
        with open(precomp, 'r') as f:
            lines = f.readlines()
        r_include = re.compile(r"^\s*#include\s*[\"<](.*)[\">]")
        for line in lines:
            m = r_include.match(line)
            if m:
                my_precomp = m.group(1)
        return get_headers(my_precomp)
    except FileNotFoundError:
        return []


def build_recursive_headers(parent, includes, headers, precomp_headers, level, output):
    if level == 0:
        return
    level -= 1

    full_precomp_headers = build_full_headers(includes, precomp_headers)
    for pair in full_precomp_headers:
        output.add(f"\"{parent[1]}\" -> \"{pair[1]}\" [color=red]\n")

    full_headers = build_full_headers(includes, headers)
    for pair in full_headers:
        if level == args.depth - 1:
            output.add(f"\"{parent[1]}\" -> \"{pair[1]}\" [color=blue]\n")
        else:
            output.add(f"\"{parent[1]}\" -> \"{pair[1]}\"\n")
        new_headers = get_headers(pair[0])
        if not pair[0].startswith("/usr/include") and "vcpkg" not in pair[0]:
            build_recursive_headers(
                pair, includes, new_headers, [], level, output)


def build_recursive_headers_explain(parent, includes, headers, precomp_headers, level, output):
    if level == 0:
        return
    level -= 1

    full_precomp_headers = build_full_headers(includes, precomp_headers)
    for pair in full_precomp_headers:
        output.add((parent[0], pair[0], True, True))

    full_headers = build_full_headers(includes, headers)
    for pair in full_headers:
        if level == args.depth - 1:
            output.add((parent[0], pair[0], True, False))
        else:
            output.add((parent[0], pair[0], False, False))
        new_headers = get_headers(pair[0])
        if not pair[0].startswith("/usr/include") and "vcpkg" not in pair[0]:
            build_recursive_headers_explain(
                pair, includes, new_headers, [], level, output)


home = os.getcwd()

parser = argparse.ArgumentParser(
    prog="deps.py", description='Draw a header dependency tree from one file.')
parser.add_argument('--file', '-f', type=str,
                    help='Specify the file whose headers are to analyze.')
parser.add_argument('--output', '-o', type=str,
                    help='Specify the DOT file to store the result graph.')
parser.add_argument('--depth', '-d', type=int, default=2,
                    help='Specify the depth to look up headers.')
parser.add_argument('--compile-commands', '-c', type=str, default="compile_commands.json",
                    help='Specify the depth to look up headers.')
parser.add_argument('--explain', '-e', action='store_true', default=False,
                    help='Explain what header to remove from the file.')
args = parser.parse_args()

if not os.path.isfile(args.compile_commands):
    print("the compile_commands.json file must be provided, by default deps looks for it in the current path, otherwise you can provide it with the -c option.", file=sys.stderr)
    sys.exit(1)

with open(args.compile_commands, "r") as f:
    js = json.load(f)

if args.file is not None:
    if args.file.startswith(home):
        full_name = (args.file, args.file)
    else:
        full_name = (home + '/' + args.file, args.file)

if not args.explain:
    output = set()
    for entry in js:
        if entry["file"] == full_name[0]:
            result = parse_command(entry)
            if 'pch' in result:
                precomp_headers = get_precomp_headers(result['pch'])
            else:
                precomp_headers = []
            includes = result['include_dirs']

            headers = get_headers(full_name[0])
            build_recursive_headers(
                full_name, includes, headers, precomp_headers, args.depth, output)
            break

    if args.output:
        output_file = args.output
    else:
        output_file = "/tmp/deps.dot"

    with open(output_file, "w") as f:
        f.write("digraph deps {\n")
        for o in output:
            f.write(o)
        f.write("}\n")

    if os.path.exists("/usr/bin/dot"):
        os.system(f"/usr/bin/dot -Tpng {output_file} -o /tmp/deps.png")
        if os.path.exists("/usr/bin/lximage-qt"):
            os.system("/usr/bin/lximage-qt /tmp/deps.png")
        elif os.path.exists("/usr/bin/eog"):
            os.system("/usr/bin/eog /tmp/deps.png")
    else:
        print(f"Output written at '{output_file}'.")
else:
    output = set()
    for entry in js:
        # A little hack so that if we specify no file, they are all analyzed.
        print(f"Analyzing '{entry['file']}'")
        if args.file is None:
            full_name = (entry["file"], entry["file"])
        # full_name is a pair with the full_name at first, and the short one at second.
        if entry["file"] == full_name[0]:
            result = parse_command(entry)
            if 'pch' in result:
                precomp_headers = get_precomp_headers(result['pch'])
            else:
                precomp_headers = []
            includes = result['include_dirs']

            headers = get_headers(full_name[0])
            build_recursive_headers_explain(
                full_name, includes, headers, precomp_headers, args.depth, output)
            selected = [o for o in output if o[2]]
            complement = [o for o in output if not o[2]]
            to_remove = []
            for o in selected:
                for oo in complement:
                    if oo[1] == o[1]:
                        to_remove.append(oo)
                        break
            if len(to_remove) > 0:
                print(
                    f"In file {YELLOW}{full_name[1]}{RESET}, there are includes coming from others headers:")
                print("-from precomp header:")
                for o in to_remove:
                    if o[3]:
                        print(f"  * {GREEN}{o[0]}{RESET}")
                print("-from others headers:")
                for o in to_remove:
                    if not o[3]:
                        print(f"  * {ORANGE}{o[0]}{RESET}")
            if args.file is not None:
                break
            else:
                output = set()
