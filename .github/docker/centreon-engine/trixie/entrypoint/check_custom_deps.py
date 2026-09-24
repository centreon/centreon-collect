#!/usr/bin/env python3
"""
Parse custom-deps.json and print a space-separated list of APT packages to install.
Package names are validated against Debian naming rules before being emitted.

Usage: check_custom_deps.py <custom_deps_json_path>
"""

import json
import re
import sys

PKG_RE = re.compile(r'^[a-z0-9][a-z0-9+\-.]+$')


def main():
    if len(sys.argv) < 2:
        print("Usage: check_custom_deps.py <custom_deps_json_path>", file=sys.stderr)
        sys.exit(1)

    try:
        with open(sys.argv[1]) as f:
            deps = json.load(f)
    except Exception as e:
        print(f"Failed to read {sys.argv[1]}: {e}", file=sys.stderr)
        sys.exit(0)

    pkgs = []
    for pkg in deps.get("apt", []):
        pkg = str(pkg).lower()
        if PKG_RE.match(pkg):
            pkgs.append(pkg)
        else:
            print(f"  {pkg}: invalid package name, skipping", file=sys.stderr)

    print(" ".join(pkgs))


if __name__ == "__main__":
    main()
