#!/usr/bin/python3

import argparse
import re
import os
import json


def parse_command(entry):
    """
    Returns all the include directories used in the compilation command.
    Args:
        entry: An entry of the compile_command.json

    Returns: A list of the directories with absolute paths.
    """
    command = entry['command']
    args = command.split(' ')

    retval = {}

    for a in args:
        if 'cmake_pch.hxx.pch' in a:
            retval['pch'] = a
            break

    # -I at first
    retval['include_dirs'] = [a[2:].strip() for a in args if a.startswith('-I')]

    # -isystem at second
    idx = args.index("-isystem")
    if idx >= 0:
        retval['include_dirs'].append(args[idx + 1])

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
            if os.path.exists(file):
                retval.append((file, header))
                break
    return retval


def get_precomp_headers(precomp):
    idx = precomp.rfind('.')
    file = precomp[:idx]
    print(file)
    with open(file, 'r') as f:
        lines = f.readlines()
    r_include = re.compile(r"^\s*#include\s*[\"<](.*)[\">]")
    for line in lines:
        m = r_include.match(line)
        if m:
            my_precomp = m.group(1)
    return get_headers(my_precomp)


home = os.getcwd()

parser = argparse.ArgumentParser(
        prog="deps.py", description='Draw a header dependency tree from one file.')
parser.add_argument('--file', '-f', type=str,
        help='Specify the file whose headers are to analyze.')
parser.add_argument('--output', '-o', type=str,
                    help='Specify the DOT file to store the result graph.')
parser.add_argument('--depth', '-d', type=int, default=2,
                    help='Specify the depth to look up headers.')
args = parser.parse_args()

with open("compile_commands.json", "r") as f:
    js = json.load(f)


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
            build_recursive_headers(pair, includes, new_headers, [], level, output)


output = set()
full_name = (home + '/' + args.file, args.file)
for entry in js:
    if entry["file"] == full_name[0]:
        result = parse_command(entry)
        if 'pch' in result:
            precomp_headers = get_precomp_headers(result['pch'])
        else:
            precomp_headers = []
        includes = result['include_dirs']

        headers = get_headers(full_name[0])
        build_recursive_headers(full_name, includes, headers, precomp_headers, args.depth, output)
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

os.system(f"/usr/bin/dot -Tpng {output_file} -o /tmp/deps.png")
if os.path.exists("/usr/bin/lximage-qt"):
    os.system("/usr/bin/lximage-qt /tmp/deps.png")
elif os.path.exists("/usr/bin/eog"):
    os.system("/usr/bin/eog /tmp/deps.png")
