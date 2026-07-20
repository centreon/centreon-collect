#!/usr/bin/env python3
"""Show a Robot test's position in the full run and the percentage already executed.

If a real `robot` run is in progress, it also estimates the remaining time from
the run's elapsed time (read from the process) and the number of finished tests.

Usage:
    ./test-progress.py BRRDCDRBU1
    ./test-progress.py -f BRRDCDRBU1     # force regeneration of the dryrun cache
    ./test-progress.py                   # auto-detect the currently running test

When no test name is given, the script reads the running run's output.xml (which
robot fills incrementally) and uses the last <test> tag it finds — i.e. the test
currently executing — exactly as if that name had been passed on the command line.

The script discovers the exact command line of the running `robot` process and
reuses its arguments (suites, tags, tests…) to build the dryrun, so it works for
any selection — e.g. `robot bam` or `robot -s some.suite`, not only a full run.
The dryrun cache is stored as /tmp/dryrun-<hash>.xml, the hash being derived from
those arguments so each distinct selection keeps its own cache. The dryrun only
parses the .robot files, so it is fast; it is regenerated when the cache is
missing or with -f/--force.

If no run is detected, it falls back to the default selection (`-e unstable .`).

Run it inside the podman container (where robot is available):
    cd /work/develop/tests && . robotframework/bin/activate && ./test-progress.py BRRDCDRBU1
"""

import argparse
import glob
import hashlib
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

# Selection used when no running robot run can be detected.
DEFAULT_ARGS = ["-e", "unstable", "."]

# Output-related options (each followed by one value) that we drop from the
# discovered command line so they cannot override our own dryrun output paths.
OUTPUT_OPTS_WITH_VALUE = {
    "-o", "--output", "-r", "--report", "-l", "--log",
    "-b", "--debugfile", "-d", "--outputdir", "-x", "--xunit",
}


def find_robot_run():
    """Return (elapsed_seconds, robot_args, pid) for the running real robot run.

    robot_args is the exact list of arguments passed to robot (everything after
    the robot script in argv). pid is the robot process id (used to locate its
    output.xml via /proc/<pid>/cwd). Returns (None, None, None) if no run is
    detected. The script's own `robot --dryrun` invocations are skipped.
    """
    pgrep = subprocess.run(["pgrep", "-f", "bin/robot"],
                           capture_output=True, text=True)
    for pid in pgrep.stdout.split():
        try:
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                argv = [a.decode() for a in f.read().split(b"\x00") if a]
        except OSError:
            continue
        ri = next((i for i, a in enumerate(argv)
                   if a == "robot" or a.endswith("/robot")), None)
        if ri is None:
            continue
        args = argv[ri + 1:]
        if "--dryrun" in args:          # our own cache generation, skip it
            continue
        et = subprocess.run(["ps", "-o", "etimes=", "-p", pid],
                            capture_output=True, text=True).stdout.strip()
        elapsed = int(et) if et.isdigit() else None
        return elapsed, args, pid
    return None, None, None


def strip_output_opts(args):
    """Remove --dryrun and output-file options (with their value) from args."""
    out = []
    skip_value = False
    for a in args:
        if skip_value:
            skip_value = False
            continue
        if a in OUTPUT_OPTS_WITH_VALUE:
            skip_value = True
            continue
        if a == "--dryrun":
            continue
        out.append(a)
    return out


# Matches the opening tag of a <test> element in robot's output.xml and captures
# its name. Robot writes `<test id="..." name="..." line="...">`; the file is not
# well-formed until the run ends, so we scan the raw bytes rather than parse it.
TEST_TAG_RE = re.compile(rb'<test\b[^>]*\bname="([^"]*)"')


def output_xml_path(run_args, pid):
    """Locate the output.xml of the running robot run.

    Honours -o/--output and -d/--outputdir from the discovered command line,
    resolving relative paths against the robot process's working directory
    (/proc/<pid>/cwd). Returns None if output is disabled (-o NONE).
    """
    output = "output.xml"
    outputdir = None
    i = 0
    while i < len(run_args):
        a = run_args[i]
        if a in ("-o", "--output") and i + 1 < len(run_args):
            output = run_args[i + 1]
            i += 2
            continue
        if a in ("-d", "--outputdir") and i + 1 < len(run_args):
            outputdir = run_args[i + 1]
            i += 2
            continue
        i += 1
    if output.upper() == "NONE":
        return None
    try:
        cwd = os.readlink(f"/proc/{pid}/cwd")
    except OSError:
        cwd = os.getcwd()
    if outputdir and not os.path.isabs(outputdir):
        outputdir = os.path.join(cwd, outputdir)
    if not os.path.isabs(output):
        output = os.path.join(outputdir or cwd, output)
    return output


def last_test_in_output(path):
    """Return the name of the last <test> tag in a (possibly partial) output.xml.

    That is the test currently executing (or the last one finished), matching the
    user's intent of "the last test whose tag is visible in the file".
    """
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    matches = TEST_TAG_RE.findall(data)
    return matches[-1].decode() if matches else None


def cache_path(robot_args):
    """Cache file path, hashed on the selection so each run keeps its own."""
    digest = hashlib.md5("\x00".join(robot_args).encode()).hexdigest()[:12]
    return f"/tmp/dryrun-{digest}.xml"


def generate_dryrun(cache, robot_args):
    """(Re)generate the cache via robot --dryrun, without running the tests.

    The XML is written to a temporary file and atomically renamed, so the cache
    is never left truncated if the dryrun is interrupted mid-write.
    """
    # Drop any stale temp files left by an interrupted previous dryrun.
    for stale in glob.glob(f"{cache}.*.tmp"):
        try:
            os.remove(stale)
        except OSError:
            pass
    # Unique temp name so concurrent invocations never clobber each other.
    tmp = f"{cache}.{os.getpid()}.tmp"
    # Our output options come first so they precede the data sources; the
    # discovered selection options/paths follow unchanged.
    cmd = (["robot", "--dryrun", "-o", tmp, "-r", "NONE", "-l", "NONE",
            "-b", "NONE"] + robot_args)
    print(f"[info] no valid cache — running a full dryrun (slow, can take a "
          f"minute):\n       {' '.join(cmd)}", file=sys.stderr)
    try:
        res = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        sys.exit("[error] 'robot' not found — activate the venv first: "
                 ". robotframework/bin/activate")
    # --dryrun returns non-zero when some keywords fail to resolve, but the XML
    # is still produced with every <test> entry, so we only bail if it is missing.
    if not os.path.exists(tmp):
        print(res.stdout, file=sys.stderr)
        print(res.stderr, file=sys.stderr)
        sys.exit(f"[error] dryrun failed (code {res.returncode}), no XML produced")
    os.replace(tmp, cache)


def load_tests(cache, robot_args, force):
    if force:
        generate_dryrun(cache, robot_args)
    for _ in range(2):
        if not os.path.exists(cache):
            generate_dryrun(cache, robot_args)
        try:
            tree = ET.parse(cache)
            return [e.get("name") for e in tree.iter("test")]
        except ET.ParseError:
            # Stale/truncated cache (e.g. an interrupted dryrun): drop it and
            # regenerate once.
            print(f"[info] cache {cache} is corrupt, regenerating…",
                  file=sys.stderr)
            try:
                os.remove(cache)
            except OSError:
                pass
    sys.exit(f"[error] could not produce a valid dryrun cache at {cache}")


def fmt_duration(seconds):
    """Format a number of seconds as e.g. '1h23m45s'."""
    seconds = int(seconds)
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"


def main():
    parser = argparse.ArgumentParser(
        description="Show a Robot test's position in the full run.")
    parser.add_argument("test", nargs="?",
                        help="test name, e.g. BRRDCDRBU1; if omitted, the "
                             "currently running test is auto-detected from the "
                             "run's output.xml")
    parser.add_argument("-f", "--force", action="store_true",
                        help="regenerate the dryrun cache even if it exists")
    args = parser.parse_args()

    # Discover the running robot run to reuse its exact selection and timing.
    elapsed, run_args, pid = find_robot_run()
    robot_args = strip_output_opts(run_args) if run_args else DEFAULT_ARGS
    cache = cache_path(robot_args)

    # No test name given: behave as if the user had typed the currently running
    # test, read from the (incrementally filled) output.xml of the live run.
    if args.test is None:
        if run_args is None:
            sys.exit("[error] no test name given and no running robot run "
                     "detected — nothing to auto-detect")
        out_xml = output_xml_path(run_args, pid)
        if not out_xml or not os.path.exists(out_xml):
            sys.exit(f"[error] no test name given and output.xml not found"
                     f"{f' at {out_xml}' if out_xml else ''}")
        args.test = last_test_in_output(out_xml)
        if not args.test:
            sys.exit(f"[error] could not detect the current test from "
                     f"{out_xml} (no <test> tag yet?)")
        print(f"[info] auto-detected current test: {args.test}",
              file=sys.stderr)

    tests = load_tests(cache, robot_args, args.force)
    total = len(tests)

    if args.test not in tests:
        sys.exit(f"[error] test '{args.test}' not found among the {total} tests "
                 f"(typo? excluded by a tag?)")

    idx = tests.index(args.test)       # 0-based
    position = idx + 1                 # 1-based
    remaining_count = total - idx      # this one included
    pct = position / total * 100

    # Timing: if a real run is in progress, estimate the remaining time from
    # the elapsed time and the number of tests already finished.
    timing = None
    if elapsed is not None and idx > 0:
        avg = elapsed / idx                # seconds per finished test
        eta = avg * remaining_count        # this one + the ones still to come
        timing = (elapsed, avg, eta)

    # Highlight the test name and the position/total fraction in distinct,
    # clearly visible colors (only on a real terminal).
    name = args.test
    fraction = f"#{position}/{total}"
    remaining = str(remaining_count)
    percent = f"{pct:.1f}"
    if sys.stdout.isatty():
        name = f"\033[1;93m{name}\033[0m"               # bright bold yellow
        fraction = f"\033[1;96m{fraction}\033[0m"       # bright bold cyan
        remaining = f"\033[1;96m{remaining}\033[0m"     # bright bold cyan
        percent = f"\033[1;38;5;208m{percent}\033[0m"   # bright bold orange

    print(f"Test        : {name}  ({fraction})")
    print(f"Remaining   : {remaining}  (current test included)")
    print(f"Progress    : {percent} %")

    if timing:
        elapsed, avg, eta = timing
        eta_str = fmt_duration(eta)
        if sys.stdout.isatty():
            eta_str = f"\033[1;38;5;208m{eta_str}\033[0m"   # bright bold orange
        print(f"Elapsed     : {fmt_duration(elapsed)}  "
              f"(~{avg:.1f}s/test)")
        print(f"ETA         : {eta_str}  (estimated time remaining)")
    elif elapsed is None:
        print("ETA         : n/a (no running robot run detected)")


if __name__ == "__main__":
    main()
