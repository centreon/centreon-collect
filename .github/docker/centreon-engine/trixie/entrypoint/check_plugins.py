#!/usr/bin/env python3
"""
Check which plugins from plugins.json need to be installed or upgraded.
Prints a space-separated list of '<pkg>-*' patterns for packages that are
missing or whose installed version does not start with the requested version.

Usage: check_plugins.py <plugins_json_path>
"""

import json
import re
import subprocess
import sys

PKG_RE = re.compile(r'^[a-z0-9][a-z0-9+\-.]+$')


def main():
    if len(sys.argv) < 2:
        print("Usage: check_plugins.py <plugins_json_path>", file=sys.stderr)
        sys.exit(1)

    plugins_json = sys.argv[1]
    try:
        with open(plugins_json) as f:
            plugins = json.load(f)
    except Exception:
        sys.exit(0)

    to_install = []
    for pkg, ver in plugins.items():
        if not PKG_RE.match(pkg):
            print(f'  {pkg}: invalid package name, skipping', file=sys.stderr)
            continue
        ver_str = str(ver)
        result = subprocess.run(
            ['dpkg-query', '-W', '-f=${Version}', pkg],
            capture_output=True, text=True
        )
        installed = result.stdout.strip()
        if result.returncode != 0 or not installed:
            print(f'  {pkg}: not installed -> queuing', file=sys.stderr)
            to_install.append(pkg + '-*')
        elif installed.startswith(ver_str):
            print(f'  {pkg}: up-to-date ({installed})', file=sys.stderr)
        else:
            print(f'  {pkg}: outdated ({installed} != {ver_str}) -> queuing', file=sys.stderr)
            to_install.append(pkg + '-*')

    print(' '.join(to_install))


if __name__ == '__main__':
    main()
