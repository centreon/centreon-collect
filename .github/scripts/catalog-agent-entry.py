#!/usr/bin/env python3
"""Insert a monitoring-agent windows build into WebApp-download's src/data/catalog.yaml.

Text surgery on purpose, not a YAML round-trip: every YAML library reformats the whole
document, which would bury a one-build change in a thousand-line diff and make review
useless. Only the two lists of the `agent > targets > id: windows` block are touched;
every other byte of the file is preserved.

  downloads:     the two featured buttons, newest of each supported train -> the entry of
                 THIS train is replaced, the other train's entry is left alone
  all_versions:  the full history -> the entry is prepended (or replaced when re-running)

Exits non-zero and writes nothing if the structure is not what is expected; the caller
then aborts before any commit.
"""
import argparse
import re
import sys

ENTRY_INDENT = " " * 8
FIELD_INDENT = " " * 10


def fail(message):
    print(f"::error::catalog-agent-entry: {message}", file=sys.stderr)
    sys.exit(1)


def render_entry(version, file_url, md5, size):
    return [
        f'{ENTRY_INDENT}- version: "{version}"\n',
        f'{FIELD_INDENT}file_url: "{file_url}"\n',
        f'{FIELD_INDENT}md5: "{md5}"\n',
        f"{FIELD_INDENT}size: {size}\n",
    ]


def find_windows_target(lines):
    """Return the [start, end) line range of the `- id: windows` target block."""
    agent = next((i for i, l in enumerate(lines) if l.startswith("agent:")), None)
    if agent is None:
        fail("no top-level `agent:` key")
    # the next top-level key closes the agent block
    end = next(
        (i for i in range(agent + 1, len(lines)) if re.match(r"^[A-Za-z_]", lines[i])),
        len(lines),
    )
    start = next(
        (i for i in range(agent, end) if lines[i].rstrip() == "    - id: windows"), None
    )
    if start is None:
        fail("no `- id: windows` target under `agent: targets:`")
    # the next sibling target closes it
    stop = next(
        (i for i in range(start + 1, end) if re.match(r"^    - id: ", lines[i])), end
    )
    return start, stop


def find_list(lines, start, stop, name):
    """Return (header_index, [entry_start_indices]) for a `name:` list in the block."""
    header = next(
        (i for i in range(start, stop) if lines[i].rstrip() == f"      {name}:"), None
    )
    if header is None:
        fail(f"no `{name}:` list in the windows target")
    header_indent = len(lines[header]) - len(lines[header].lstrip())
    entries = []
    for i in range(header + 1, stop):
        line = lines[i]
        if line.strip() == "" or line.lstrip().startswith("#"):
            continue
        indent = len(line) - len(line.lstrip())
        if line.startswith(f"{ENTRY_INDENT}- version:"):
            entries.append(i)
        elif line.startswith(FIELD_INDENT):
            continue
        elif line.startswith(f"{ENTRY_INDENT}- "):
            fail(
                f"`{name}:` has an entry whose first key is not `version` ({lines[i].strip()!r}). "
                "This editor keys on version; fix the entry or teach it the new shape."
            )
        elif indent > header_indent or (indent == header_indent and line.lstrip().startswith("- ")):
            # still inside the list but not a shape we handle (a differently indented sequence,
            # a nested mapping): inserting here would duplicate or corrupt, so refuse
            fail(
                f"`{name}:` is laid out in a way this editor does not understand "
                f"({lines[i].rstrip()!r}). Refusing to guess."
            )
        else:
            break
    return header, entries


def entry_end(lines, index, stop):
    """Line after the last field of the entry beginning at `index`.

    Comments and blank lines inside an entry belong to it: stopping at the first of them
    would leave the entry's remaining fields orphaned below the replacement, publishing a
    file_url with the previous md5.
    """
    end = index + 1
    last_field = end
    while end < stop:
        line = lines[end]
        if line.startswith(FIELD_INDENT):
            end += 1
            last_field = end
        elif line.strip() == "" or line.lstrip().startswith("#"):
            end += 1
        else:
            break
    return last_field


def version_of(line):
    m = re.search(r'- version:\s*"([^"]+)"', line)
    return m.group(1) if m else None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--catalog", required=True)
    p.add_argument("--version", required=True, help="e.g. 25.10.9")
    p.add_argument("--file-url", required=True)
    p.add_argument("--md5", required=True)
    p.add_argument("--size", required=True)
    args = p.parse_args()

    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        fail(f"version must be MAJOR.MINOR.PATCH (got '{args.version}')")
    if not re.fullmatch(r"[0-9a-f]{32}", args.md5):
        fail(f"md5 must be 32 lowercase hex chars (got '{args.md5}')")
    if not re.fullmatch(r"[1-9][0-9]*", args.size):
        fail(f"size must be a positive integer (got '{args.size}')")
    train = args.version.rsplit(".", 1)[0]

    with open(args.catalog, encoding="utf-8") as fh:
        lines = fh.readlines()

    new_entry = render_entry(args.version, args.file_url, args.md5, args.size)
    changed = []

    # all_versions first: editing the lower list would shift the indices of the upper one
    for name, same_train_only in (("all_versions", False), ("downloads", True)):
        start, stop = find_windows_target(lines)
        header, entries = find_list(lines, start, stop, name)

        replace_at = None
        for i in entries:
            found = version_of(lines[i])
            if found == args.version:
                replace_at = i  # re-run: replace in place, stays idempotent
                break
            if same_train_only and found and found.rsplit(".", 1)[0] == train:
                replace_at = i  # this train's featured entry
                break

        if replace_at is not None:
            end = entry_end(lines, replace_at, stop)
            known = {"version", "file_url", "md5", "size"}
            for line in lines[replace_at:end]:
                key = re.match(r"\s*-?\s*([A-Za-z_][\w-]*):", line)
                if key and key.group(1) not in known:
                    fail(
                        f"the `{name}:` entry being replaced carries `{key.group(1)}`, which this "
                        "editor does not render - replacing it would drop that field."
                    )
            if lines[replace_at:end] == new_entry:
                changed.append(f"{name}: unchanged")
                continue
            previous = version_of(lines[replace_at]) or "?"
            lines[replace_at:end] = new_entry
            changed.append(f"{name}: replaced {previous}")
        else:
            lines[header + 1 : header + 1] = new_entry
            changed.append(f"{name}: inserted")

    with open(args.catalog, "w", encoding="utf-8") as fh:
        fh.writelines(lines)

    print(f"catalog.yaml {args.version}: " + ", ".join(changed))

    # The Agent tab shows one featured button per supported train, two today. A new train adds
    # a third rather than silently dropping someone's release: say so loudly instead, and let
    # the reviewer of the pull request decide which train stops being featured.
    start, stop = find_windows_target(lines)
    _, featured = find_list(lines, start, stop, "downloads")
    if len(featured) != 2:
        print(
            f"::warning::agent.targets[windows].downloads now has {len(featured)} featured "
            f"entries ({', '.join(version_of(lines[i]) or '?' for i in featured)}). The Agent tab "
            "features one build per supported train - decide which trains belong there before merging."
        )
    print(f"featured_count={len(featured)}")


if __name__ == "__main__":
    main()
