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
from collections import defaultdict

ESC = '\x1b'
YELLOW = ESC + '[1;33m'
CYAN = ESC + '[1;36m'
GREEN = ESC + '[1;32m'
RESET = ESC + '[0m'

home = os.getcwd()

parser = argparse.ArgumentParser(
    prog="deps.py", description='Draw a header dependency tree from one file.')
parser.add_argument('filename', nargs='+',
                    help='Specify the file whose headers are to analyze.')
parser.add_argument('--output', '-o', type=str,
                    help='Specify the DOT file to store the result graph.')
parser.add_argument('--depth', '-d', type=int, default=2,
                    help='Specify the depth to look up headers.')
parser.add_argument('--compile-commands', '-c', type=str, default="compile_commands.json",
                    help='Specify the depth to look up headers.')
parser.add_argument('--explain', '-e', action='store_true', default=False,
                    help='Explain what header to remove from the file.')
parser.add_argument('--fix', '-f', action='store_true', default=False,
                    help='Removed headers that seems not needed (this action is dangerous).')
args = parser.parse_args()


# Graph class to represent a directed graph
class Graph:
    def __init__(self):
        self.graph = defaultdict(list)

    # Method to add an edge between two nodes u and v (strings)
    def add_edge(self, u, v):
        self.graph[u].append(v)

    # Method to find all paths from source to destination
    def find_all_paths(self, source, destination):
        paths = []
        current_path = []

        # Check for a direct path (single edge path)
        if destination in self.graph[source]:
            paths.append([source, destination])

        # Find all other paths using DFS
        self.dfs(source, destination, current_path, paths)

        return paths

    # Recursive DFS function to explore paths
    def dfs(self, current_node, destination, current_path, paths):
        # Add the current node to the current path
        current_path.append(current_node)

        # If we reach the destination node, store the current path
        if current_node == destination and len(current_path) > 2:  # Ensure this is not the single-edge path
            paths.append(list(current_path))
        else:
            # Otherwise, explore all neighbors of the current node
            for neighbor in self.graph[current_node]:
                if neighbor not in current_path:  # Avoid immediate cycles
                    self.dfs(neighbor, destination, current_path, paths)

        # Backtrack: remove the current node from the path before returning
        current_path.pop()

    # Method to check if at least two paths exist from source to destination
    def find_paths_with_at_least_two(self, source, destination):
        all_paths = self.find_all_paths(source, destination)

        # Return paths only if there are at least two
        if len(all_paths) >= 2:
            return all_paths
        else:
            return None

    # Method to find all node pairs with at least two paths
    def find_all_pairs_with_at_least_two_paths(self):
        result = {}
        destinations = set()
        # We get all the leafs.
        for v in self.graph.values():
            destinations.update(v)
        nodes = set(self.graph.keys())  # All the nodes except leafs.
        destinations.update(nodes) # And the union of them.
        # Iterate through all pairs of nodes in the graph
        for source in nodes:
            for destination in destinations:
                if source != destination:
                    paths = self.find_paths_with_at_least_two(source, destination)
                    if paths:
                        for path in paths:
                            if len(path) == 2:
                                result[(source, destination)] = paths
        return result


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
    global args
    if level == 0:
        return

    if level == args.depth:
        full_precomp_headers = build_full_headers(includes, precomp_headers)
        for pair in full_precomp_headers:
            output.add_edge(parent[0], pair[0])

    level -= 1
    full_headers = build_full_headers(includes, headers)
    for pair in full_headers:
        output.add_edge(parent[0], pair[0])
        new_headers = get_headers(pair[0])
        if not pair[0].startswith("/usr/include") and "vcpkg" not in pair[0] and ".pb." not in pair[0]:
            build_recursive_headers_explain(
                pair, includes, new_headers, [], level, output)


def remove_header_from_file(header, filename):
    print(f"  * {YELLOW}{header}{RESET} removed from {CYAN}{filename}{RESET}.")
    r = re.compile(r"^#include\s*[\"<](.*)[\">]")
    with open(filename, "r") as f:
        lines = f.readlines()
    with open(filename, "w") as f:
        for l in lines:
            ls = l.strip()
            if l.startswith("#include"):
                m = r.match(ls)
                if m and header.endswith(m.group(1)):
                    continue
            f.write(l)


if not os.path.isfile(args.compile_commands):
    print("the compile_commands.json file must be provided, by default deps looks for it in the current path, otherwise you can provide it with the -c option.", file=sys.stderr)
    sys.exit(1)

with open(args.compile_commands, "r") as f:
    js = json.load(f)

# An array of pairs with (fullname, shortname) of the files given on the command line.
filename = []
# new_js contains only files matching those in filename.
new_js = []
for f in args.filename:
    if f.startswith(home):
        full_name = (f, f)
    else:
        full_name = (home + '/' + f, f)
    for ff in js:
        if ff['file'] == full_name[0]:
            filename.append(full_name)
            new_js.append(ff)

if not args.explain:
    for full_name in filename:
        output = set()
        for entry in new_js:
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
    for full_name in filename:
        print(f"Analyzing '{full_name[0]}'")
        output = Graph()
        for entry in new_js:
            # A little hack so that if we specify no file, they are all analyzed.
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

                result = output.find_all_pairs_with_at_least_two_paths()
                if result:
                    if args.fix:
                        for (source, destination), paths in result.items():
                            remove_header_from_file(destination, source)
                    else:
                        print(f"{GREEN}{full_name[0]}{RESET}:")
                        for (source, destination), paths in result.items():
                            print(f"  * {YELLOW}{destination}{RESET} can be removed from {CYAN}{source}{RESET}.")
                break
