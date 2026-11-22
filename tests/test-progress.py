#!/usr/bin/env python3
"""Show a Robot test's position in the full run and the percentage already executed.

If a real `robot` run is in progress, it also estimates the remaining time from
the run's elapsed time (read from the process) and the number of finished tests.

Usage:
    ./test-progress.py BRRDCDRBU1
    ./test-progress.py -f BRRDCDRBU1     # force regeneration of the dryrun cache
    ./test-progress.py                   # auto-detect the currently running test
    ./test-progress.py -p                # follow the run live with a progress bar

With -p/--progress the script keeps running while the robot run is alive,
refreshing the display every few seconds (like `watch`) and drawing a progress
bar across the terminal width. It exits as soon as the run finishes.

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
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

# Selection used when no running robot run can be detected.
DEFAULT_ARGS = ["-e", "unstable", "."]

# Refresh period of the -p/--progress mode, in seconds (same feel as `watch`).
REFRESH_INTERVAL = 2.0

# Output-related options (each followed by one value) that we drop from the
# discovered command line so they cannot override our own dryrun output paths.
OUTPUT_OPTS_WITH_VALUE = {
    "-o", "--output", "-r", "--report", "-l", "--log",
    "-b", "--debugfile", "-d", "--outputdir", "-x", "--xunit",
}


def find_robot_run():
    """Find the running real robot run (dryrun invocations are skipped).

    Returns:
        A tuple (elapsed_seconds, robot_args, pid): the run's elapsed time in
        seconds, the exact list of arguments passed to robot (everything after
        the robot script in argv) and the robot process id (used to locate its
        output.xml via /proc/<pid>/cwd). (None, None, None) if no run is
        detected.
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
    """Remove --dryrun and output-file options (with their value) from args.

    Args:
        args: The robot command-line arguments to filter.

    Returns:
        A new list of arguments without --dryrun nor the options listed in
        OUTPUT_OPTS_WITH_VALUE (each removed together with its value).
    """
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

# Matches a <status status="..."> tag and captures the status word (PASS, FAIL,
# SKIP, NOT RUN). Both keywords and the test itself carry one; inside a
# <test>…</test> block the test's own status is the last one (written just before
# </test>), which is what we key on.
STATUS_TAG_RE = re.compile(rb'<status\b[^>]*\bstatus="([A-Z ]+)"')


def output_xml_path(run_args, pid):
    """Locate the output.xml of the running robot run.

    Honours -o/--output and -d/--outputdir from the discovered command line,
    resolving relative paths against the robot process's working directory
    (/proc/<pid>/cwd).

    Args:
        run_args: The arguments of the running robot process.
        pid: The pid of the running robot process.

    Returns:
        The absolute path of the run's output.xml, or None if output is
        disabled (-o NONE).
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

    Args:
        path: Path of the output.xml file to scan.

    Returns:
        The name of the last <test> tag, or None if the file is unreadable or
        contains no <test> tag yet.
    """
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    matches = TEST_TAG_RE.findall(data)
    return matches[-1].decode() if matches else None


class TailTestScanner:
    """Incrementally track the current test and each finished test's status.

    Robot appends to output.xml as it runs and the file can grow very large, so
    re-reading it fully at every refresh gets expensive. Instead, remember the
    offset already scanned and only read the bytes appended since, buffering
    them until a complete `<test>…</test>` block is available. Each block yields
    the test name and its result (the block's last <status>, i.e. the test-level
    one). The buffer only ever holds the bytes of the test currently running, so
    it stays small. The still-open <test> tag left in the buffer gives the test
    in progress.
    """

    _CLOSE = b"</test>"

    def __init__(self, path):
        """Constructor.

        Args:
            path: Path of the output.xml file to follow.
        """
        self._path = path
        self._offset = 0
        self._buf = b""
        self._last = None
        self._results = {}          # test name -> status word (PASS/FAIL/…)

    def scan(self):
        """Scan the bytes appended to the file since the previous call.

        Returns:
            A tuple (current, results): the name of the last <test> tag seen so
            far (the test in progress, or None), and the dict mapping each
            finished test name to its status word.
        """
        try:
            if os.path.getsize(self._path) < self._offset:
                # File truncated/replaced (new run?): rescan from the start.
                self._offset, self._buf = 0, b""
                self._last, self._results = None, {}
            with open(self._path, "rb") as f:
                f.seek(self._offset)
                data = f.read()
        except OSError:
            return self._last, self._results
        if not data:
            return self._last, self._results
        self._offset += len(data)
        self._buf += data
        # Consume every complete test block; the test-level status is the last
        # <status> in the block (keyword statuses come before it).
        while True:
            close = self._buf.find(self._CLOSE)
            if close == -1:
                break
            block = self._buf[:close]
            self._buf = self._buf[close + len(self._CLOSE):]
            names = TEST_TAG_RE.findall(block)
            if names:
                name = names[-1].decode()
                statuses = STATUS_TAG_RE.findall(block)
                self._results[name] = statuses[-1].decode() if statuses else ""
                self._last = name
        # The test still open (no </test> yet) is the one in progress.
        open_names = TEST_TAG_RE.findall(self._buf)
        if open_names:
            self._last = open_names[-1].decode()
        return self._last, self._results


def cache_path(robot_args):
    """Build the dryrun cache file path for a robot selection.

    Args:
        robot_args: The robot arguments defining the selection; hashed so
            each distinct selection keeps its own cache.

    Returns:
        The cache file path, e.g. /tmp/dryrun-0123456789ab.xml.
    """
    digest = hashlib.md5("\x00".join(robot_args).encode()).hexdigest()[:12]
    return f"/tmp/dryrun-{digest}.xml"


def generate_dryrun(cache, robot_args):
    """(Re)generate the cache via robot --dryrun, without running the tests.

    The XML is written to a temporary file and atomically renamed, so the cache
    is never left truncated if the dryrun is interrupted mid-write.

    Args:
        cache: Path of the cache file to produce.
        robot_args: The robot arguments defining the selection to dryrun.
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
    """Load the ordered list of test names from the dryrun cache.

    The cache is (re)generated when missing, corrupt or when force is set.

    Args:
        cache: Path of the dryrun cache file.
        robot_args: The robot arguments defining the selection to dryrun.
        force: Regenerate the cache even if it already exists.

    Returns:
        The list of test names, in execution order.
    """
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
    """Format a duration in a compact human-readable form.

    Args:
        seconds: The duration, in seconds.

    Returns:
        The formatted duration, e.g. '1h23m45s', '3m07s' or '42s'.
    """
    seconds = int(seconds)
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"


def status_lines(test_name, idx, total, elapsed, colour, finished_count=None):
    """Build the status report for a test as a list of printable lines.

    Args:
        test_name: Name of the test to report on.
        idx: The test's 0-based position among the selected tests.
        total: Total number of selected tests.
        elapsed: Elapsed time of the running run in seconds, or None if no
            run is in progress (disables the ETA estimation).
        colour: Use ANSI colors in the produced lines.
        finished_count: Number of tests whose result is already known. When
            given, "Remaining" is derived from it so the final frame (where the
            current test's own </test> has been scanned) reaches 0; during a
            live run it equals idx, leaving the count unchanged. Defaults to
            idx (the current test still counts as remaining).

    Returns:
        A tuple (lines, pct): the report as a list of printable lines, and
        the progress percentage so callers reuse it without recomputing it.
    """
    position = idx + 1                 # 1-based
    if finished_count is None:
        finished_count = idx           # current test not yet finished
    remaining_count = total - finished_count
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
    name = test_name
    fraction = f"#{position}/{total}"
    remaining = str(remaining_count)
    percent = f"{pct:.1f}"
    if colour:
        name = f"\033[1;93m{name}\033[0m"               # bright bold yellow
        fraction = f"\033[1;96m{fraction}\033[0m"       # bright bold cyan
        remaining = f"\033[1;96m{remaining}\033[0m"     # bright bold cyan
        percent = f"\033[1;38;5;208m{percent}\033[0m"   # bright bold orange

    lines = [f"Test        : {name}  ({fraction})",
             f"Remaining   : {remaining}  (current test included)",
             f"Progress    : {percent} %"]

    if timing:
        elapsed, avg, eta = timing
        eta_str = fmt_duration(eta)
        if colour:
            eta_str = f"\033[1;38;5;208m{eta_str}\033[0m"   # bright bold orange
        lines.append(f"Elapsed     : {fmt_duration(elapsed)}  "
                     f"(~{avg:.1f}s/test)")
        lines.append(f"ETA         : {eta_str}  (estimated time remaining)")
    elif elapsed is None:
        lines.append("ETA         : n/a (no running robot run detected)")
    return lines, pct


def progress_bar(pct, width, colour, failed_idx=frozenset(), total=0):
    """Render a progress bar, e.g. [██▉█░░░]  42.0 %.

    The filled part is green, except the columns mapping to a failed test, which
    are red — so a failure shows up as a red slice at its position in the run.

    Args:
        pct: Progress percentage (0-100).
        width: Total width of the rendered bar, in terminal columns.
        colour: Use ANSI colors in the rendered bar.
        failed_idx: 0-based positions (in the full selection) of the tests that
            failed so far.
        total: Total number of selected tests (to map a test position to a bar
            column). 0 disables the per-test coloring.

    Returns:
        The progress bar as a single printable line.
    """
    label = f" {pct:5.1f} %"
    bar_width = max(10, width - len(label) - 2)
    filled = round(bar_width * pct / 100)
    if not colour:
        return f"[{'█' * filled}{'░' * (bar_width - filled)}]{label}"

    # Column a failed test lands on (same proportional mapping as `filled`).
    failed_cols = {j * bar_width // total for j in failed_idx} if total else set()
    green, red, dim, reset = "\033[1;32m", "\033[1;31m", "\033[2m", "\033[0m"
    # Emit the filled part as color runs so a red slice stands out without one
    # escape per cell.
    segments, c = [], 0
    while c < filled:
        is_failed = c in failed_cols
        start = c
        while c < filled and (c in failed_cols) == is_failed:
            c += 1
        segments.append(f"{red if is_failed else green}{'█' * (c - start)}{reset}")
    bar = f"{''.join(segments)}{dim}{'░' * (bar_width - filled)}{reset}"
    label = f"\033[1;38;5;208m{label}\033[0m"
    return f"[{bar}]{label}"


def progress_loop(tests, run_args, pid):
    """Follow the running robot run, refreshing the display until it ends.

    Redraws a status report plus a terminal-wide progress bar every
    REFRESH_INTERVAL seconds, like `watch` would, and returns when the robot
    process disappears.

    Args:
        tests: The ordered list of test names of the selection.
        run_args: The arguments of the running robot process.
        pid: The pid of the running robot process.
    """
    out_xml = output_xml_path(run_args, pid)
    if not out_xml:
        sys.exit("[error] --progress: the run writes no output.xml (-o NONE), "
                 "cannot follow it")
    colour = sys.stdout.isatty()
    scanner = TailTestScanner(out_xml)
    # 0-based position of each test name (first occurrence, like list.index),
    # so each refresh does O(1) lookups instead of scanning the whole list.
    index = {}
    for i, name in enumerate(tests):
        index.setdefault(name, i)
    total = len(tests)
    current = None                     # last test successfully detected
    elapsed = None                     # last known elapsed time of the run
    if colour:
        sys.stdout.write("\033[?25l")  # hide the cursor while refreshing
    try:
        while True:
            # Poll the followed process directly: gone => the run has ended.
            et = subprocess.run(["ps", "-o", "etimes=", "-p", pid],
                                capture_output=True, text=True).stdout.strip()
            # The last test's closing </test> (with its status) is written just
            # before the process exits, i.e. during our sleep. So even once the
            # run is gone we must scan and render one final frame, otherwise the
            # last test stays stuck in its "in progress" state and its result
            # never lands in `results`/`failed_idx`.
            finished = not et.isdigit()
            if not finished:
                elapsed = int(et)
            detected, results = scanner.scan()
            if detected in index:
                current = detected
            # 0-based positions of the tests that failed so far.
            failed_idx = frozenset(index[n] for n, st in results.items()
                                   if st == "FAIL" and n in index)
            width = shutil.get_terminal_size().columns
            header = time.strftime("Following robot run — %H:%M:%S "
                                   "(Ctrl-C to stop watching)")
            if current is None:
                body = ["Waiting for the first test to appear in "
                        f"{out_xml}…"]
                pct = 0.0
            else:
                finished_count = sum(1 for n in results if n in index)
                body, pct = status_lines(current, index[current], total,
                                         elapsed, colour, finished_count)
            if failed_idx:
                # Failed test names, in run order (their position in the run).
                failed_names = sorted(
                    (n for n, st in results.items()
                     if st == "FAIL" and n in index),
                    key=lambda n: index[n])
                count_str = str(len(failed_idx))
                label = "Failures    : "
                # Fit the list within the terminal width, cutting it with an
                # ellipsis when it does not fit ("…" at the end).
                avail = width - len(label) - len(count_str) - 2   # 2 = "  "
                names_str = ", ".join(failed_names)
                if avail < 1:
                    names_str = ""
                elif len(names_str) > avail:
                    names_str = names_str[:avail - 1] + "…"
                sep = "  " if names_str else ""
                if colour:
                    count_str = f"\033[1;31m{count_str}\033[0m"    # bright bold red
                    if names_str:
                        names_str = f"\033[1;31m{names_str}\033[0m"
                body.append(f"{label}{count_str}{sep}{names_str}")
            screen = [header, ""] + body + [
                "", progress_bar(pct, width, colour, failed_idx, total)]
            # Home + clear-below redraws in place without full-screen flicker.
            sys.stdout.write("\033[H\033[J" if colour else "")
            sys.stdout.write("\n".join(screen) + "\n")
            sys.stdout.flush()
            if finished:
                break
            time.sleep(REFRESH_INTERVAL)
    except KeyboardInterrupt:
        print()
        print("[info] watching stopped — the robot run is still going.",
              file=sys.stderr)
        return
    finally:
        if colour:
            sys.stdout.write("\033[?25h")  # restore the cursor
            sys.stdout.flush()
    print("[info] robot run finished — exiting.", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Show a Robot test's position in the full run.")
    parser.add_argument("test", nargs="?",
                        help="test name, e.g. BRRDCDRBU1; if omitted, the "
                             "currently running test is auto-detected from the "
                             "run's output.xml")
    parser.add_argument("-f", "--force", action="store_true",
                        help="regenerate the dryrun cache even if it exists")
    parser.add_argument("-p", "--progress", action="store_true",
                        help="follow the running run live: refresh the display "
                             "with a progress bar until the run finishes")
    args = parser.parse_args()

    # Discover the running robot run to reuse its exact selection and timing.
    elapsed, run_args, pid = find_robot_run()
    robot_args = strip_output_opts(run_args) if run_args else DEFAULT_ARGS
    cache = cache_path(robot_args)

    # Live-follow mode: refresh until the run ends, then hand back the shell.
    if args.progress:
        if run_args is None:
            sys.exit("[error] --progress: no running robot run detected")
        if args.test:
            print("[info] --progress follows the running test; ignoring "
                  f"'{args.test}'", file=sys.stderr)
        tests = load_tests(cache, robot_args, args.force)
        progress_loop(tests, run_args, pid)
        return

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

    if args.test not in tests:
        sys.exit(f"[error] test '{args.test}' not found among the {len(tests)} "
                 f"tests (typo? excluded by a tag?)")

    lines, _ = status_lines(args.test, tests.index(args.test), len(tests),
                            elapsed, sys.stdout.isatty())
    print("\n".join(lines))


if __name__ == "__main__":
    main()
