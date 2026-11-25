#!/usr/bin/python3
#
# Copyright 2026 Centreon
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
"""What a benchmark result has to carry to stay interpretable later.

A figure without its context is not a measurement: which commit, was the working
tree clean, how many CPUs did the machine have, was it the container or the host.
Two runs whose contexts differ are simply not comparable, and the only way to
notice that months later is to have stored the context alongside the numbers.
"""

import datetime
import os
import socket
import subprocess
from typing import Optional


def now_iso() -> str:
    """Return the current local time, to the second.

    Returns:
        An ISO 8601 string, e.g. "2026-08-20T14:03:59".
    """
    return datetime.datetime.now().replace(microsecond=0).isoformat()


def repo_root(start: Optional[str] = None) -> Optional[str]:
    """Find the git working tree holding a path.

    Args:
        start (str, optional): where to start looking. Defaults to None, this
            file's own directory.

    Returns:
        The absolute path of the working tree root, or None outside of git.
    """
    start = start or os.path.dirname(os.path.abspath(__file__))
    try:
        out = subprocess.run(["git", "-C", start, "rev-parse", "--show-toplevel"],
                             capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return None
    return out.stdout.strip() or None


def git_info(root: Optional[str] = None) -> dict:
    """Describe the state of the source tree.

    Args:
        root (str, optional): the working tree. Defaults to None, found from
            this file.

    Returns:
        A dict with branch, commit and dirty. dirty is the one that matters: a
        measurement taken on a modified working tree is comparable to nothing,
        since nobody can reproduce what was built.
    """
    root = root or repo_root()
    info: dict = {"git_branch": None, "git_commit": None, "git_dirty": None}
    if root is None:
        return info

    def git(*args) -> Optional[str]:
        try:
            out = subprocess.run(["git", "-C", root, *args],
                                 capture_output=True, text=True, timeout=15)
        except (OSError, subprocess.SubprocessError):
            return None
        if out.returncode != 0:
            return None
        return out.stdout.strip()

    info["git_branch"] = git("rev-parse", "--abbrev-ref", "HEAD")
    info["git_commit"] = git("rev-parse", "--short=12", "HEAD")
    status = git("status", "--porcelain", "--untracked-files=no")
    # An untracked file changes nothing to the binary that is being measured, so
    # it does not make the tree dirty for our purpose -- results/ and the
    # generated configurations live there.
    info["git_dirty"] = None if status is None else bool(status)
    return info


def container_id() -> Optional[str]:
    """Tell whether we run in a container, and which one.

    Returns:
        The container hostname when one is detected, None on a bare machine.
        Robot benchmarks run inside podman while the sources are on the host, and
        the two do not have the same number of CPUs -- enough to make their
        figures incomparable.
    """
    for marker in ("/run/.containerenv", "/.dockerenv"):
        if os.path.exists(marker):
            return socket.gethostname()
    return None


def cpu_count() -> int:
    """Return the number of CPUs usable by the current process.

    Returns:
        The size of the affinity mask, which is what actually bounds a benchmark
        -- os.cpu_count() would report the whole machine even when the container
        is pinned to a subset.
    """
    try:
        return len(os.sched_getaffinity(0))
    except AttributeError:
        return os.cpu_count() or 1


def describe(root: Optional[str] = None) -> dict:
    """Collect everything a run should remember about its environment.

    Args:
        root (str, optional): the working tree. Defaults to None.

    Returns:
        A dict ready to be passed to BenchDB.start_run(env=...).
    """
    env = {"host": socket.gethostname(),
           "container": container_id(),
           "cpu_count": cpu_count()}
    env.update(git_info(root))
    return env
