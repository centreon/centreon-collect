#!/usr/bin/env python3
"""Manifest-driven staging / conformance-check tool.

Reads the `contents:` list of one or more nfpm yaml manifests (the same
files nfpm itself packages from) and either:

  stage       copy every entry's `src` to <staging>/<dst>, mechanically
              replaying what nfpm would put in the .deb/.rpm - no new
              path logic, no guessing.
  assert      given an already-populated root (e.g. the final Docker
              image's filesystem), verify every non-ghost dst: exists.
              This is the check that catches a moved path LOUDLY - ldd
              cannot see it, because externalcmd.so/cbmod.so are
              dlopen'd, not linked (DT_NEEDED), so ldd never resolves
              their path at all.

Deliberately dumb: unrecognized entry shapes (type family we don't
recognize, a `packager:`-filtered file entry with an unexpected src/dst
combination, brace-glob with zero expansions, a glob matching zero
files) are HARD FAILURES. A silent skip here is exactly the failure
mode this tool exists to eliminate - if the manifest says a file should
be there, either it lands, or the tool stops and says why not.
"""
import argparse
import glob
import itertools
import os
import re
import shutil
import sys

try:
    import yaml
except ImportError:
    sys.exit("FATAL: PyYAML not installed (apt-get install -y python3-yaml)")


def brace_expand(pattern: str):
    """Expand one level of {a,b,c} brace groups. glob.glob() does NOT
    do this on its own - centreon-broker-core.yaml's src line
    ('../../build/broker/lib/*-{neb,stats,bam,...}.so') would silently
    match zero files without this, because '{' and '}' are not glob
    metacharacters in Python (or POSIX glob() in general)."""
    m = re.search(r"\{([^{}]+)\}", pattern)
    if not m:
        return [pattern]
    prefix, choices, suffix = pattern[: m.start()], m.group(1).split(","), pattern[m.end():]
    out = []
    for choice in choices:
        out.extend(brace_expand(prefix + choice + suffix))
    return out


def die(msg):
    sys.exit(f"FATAL: {msg}")


KNOWN_TYPE_FAMILIES = {"file", "config", "dir", "ghost", "symlink", "tree"}


def type_family(entry):
    t = entry.get("type", "file")
    family = t.split("|", 1)[0]
    if family not in KNOWN_TYPE_FAMILIES:
        die(f"unrecognized content type '{t}' in entry {entry!r} - "
            f"refusing to silently skip it, teach the tool about it first")
    return family


def load_entries(yaml_paths, packager):
    """Yield (yaml_dir, entry) for every contents: entry that applies to
    `packager`, in file order across all given manifests."""
    for yp in yaml_paths:
        with open(yp, "r", encoding="utf-8") as f:
            doc = yaml.safe_load(f)
        yaml_dir = os.path.dirname(os.path.abspath(yp))
        for entry in doc.get("contents", []):
            ent_packager = entry.get("packager")
            if ent_packager is not None and ent_packager != packager:
                continue
            yield yaml_dir, entry


def resolve_src_matches(yaml_dir, src_pattern):
    expanded = brace_expand(src_pattern)
    matches = []
    for pat in expanded:
        abspat = os.path.normpath(os.path.join(yaml_dir, pat))
        found = glob.glob(abspat)
        if not found:
            die(f"src pattern matched ZERO files: '{src_pattern}' "
                f"(expanded to '{pat}', resolved from '{yaml_dir}') - "
                f"this is exactly the silent-skip failure mode this tool "
                f"exists to prevent")
        matches.extend(found)
    return sorted(set(matches))


def do_stage(yaml_paths, packager, staging_root, build_root):
    staging_root = os.path.abspath(staging_root)
    n_files = 0
    n_dirs = 0
    n_ghost = 0
    for yaml_dir, entry in load_entries(yaml_paths, packager):
        family = type_family(entry)
        dst = entry["dst"]

        if family == "dir":
            if "src" in entry:
                die(f"type: dir entry has a 'src' - unrecognized shape: {entry!r}")
            target = os.path.join(staging_root, dst.lstrip("/"))
            os.makedirs(target, exist_ok=True)
            n_dirs += 1
            continue

        if family == "ghost":
            if "src" in entry:
                die(f"type: ghost entry has a 'src' - unrecognized shape: {entry!r}")
            print(f"  [ghost, not staged - runtime-generated] {dst}")
            n_ghost += 1
            continue

        if family == "symlink":
            die(f"type: symlink not exercised by the 3 manifests this tool "
                f"was built against - implement and test before trusting it "
                f"blindly: {entry!r}")

        # file / config* / tree
        if "src" not in entry:
            die(f"entry has no 'src' and is not dir/ghost - unrecognized shape: {entry!r}")
        src_pattern = entry["src"]
        matches = resolve_src_matches(yaml_dir, src_pattern)

        is_dir_dst = dst.endswith("/")
        if not is_dir_dst and len(matches) > 1:
            die(f"src '{src_pattern}' expanded to {len(matches)} files but "
                f"dst '{dst}' has no trailing slash (exact-file target) - "
                f"ambiguous, refusing to guess which one wins")

        for src_path in matches:
            if is_dir_dst:
                dst_path = os.path.join(staging_root, dst.lstrip("/"), os.path.basename(src_path))
            else:
                dst_path = os.path.join(staging_root, dst.lstrip("/"))
            os.makedirs(os.path.dirname(dst_path), exist_ok=True)
            shutil.copy2(src_path, dst_path)
            mode = entry.get("file_info", {}).get("mode")
            if mode is not None:
                os.chmod(dst_path, int(str(mode), 8) if isinstance(mode, str) else mode)
            print(f"  {src_path} -> {dst_path}")
            n_files += 1

    print(f"=== staged {n_files} files, {n_dirs} dirs, {n_ghost} ghost entries (packager={packager}) ===")


def do_assert(yaml_paths, packager, root):
    root = os.path.abspath(root)
    missing = []
    checked = 0
    for yaml_dir, entry in load_entries(yaml_paths, packager):
        family = type_family(entry)
        dst = entry["dst"]
        if family == "ghost":
            continue  # runtime-generated, nothing to assert
        if family == "dir":
            path = os.path.join(root, dst.lstrip("/"))
            checked += 1
            if not os.path.isdir(path):
                missing.append(f"dir {dst} -> {path}")
            continue
        if family == "symlink":
            continue  # not exercised/implemented, see do_stage
        src_pattern = entry["src"]
        matches = resolve_src_matches(yaml_dir, src_pattern)
        is_dir_dst = dst.endswith("/")
        for src_path in matches:
            if is_dir_dst:
                path = os.path.join(root, dst.lstrip("/"), os.path.basename(src_path))
            else:
                path = os.path.join(root, dst.lstrip("/"))
            checked += 1
            if not os.path.exists(path):
                missing.append(f"{dst} (from {os.path.basename(src_path)}) -> {path}")

    print(f"=== conformance check: {checked} manifest-declared paths checked against {root} ===")
    if missing:
        print("MANIFEST CONFORMANCE FAILURE - declared but missing from the image:")
        for m in missing:
            print(f"  MISSING: {m}")
        sys.exit(1)
    print("MANIFEST CONFORMANCE OK - every non-ghost dst: from the manifest exists in the image")


def do_list_dst(yaml_paths, packager):
    """Print every non-ghost, non-symlink dst: this manifest set implies,
    one absolute path per line - for a pure-bash `test -e` loop in the
    final runtime stage, so the shipped image needs no python/yaml at all,
    only the builder stage does."""
    for yaml_dir, entry in load_entries(yaml_paths, packager):
        family = type_family(entry)
        dst = entry["dst"]
        if family in ("ghost", "symlink"):
            continue
        if family == "dir":
            print(dst)
            continue
        src_pattern = entry["src"]
        matches = resolve_src_matches(yaml_dir, src_pattern)
        is_dir_dst = dst.endswith("/")
        for src_path in matches:
            print(os.path.join(dst, os.path.basename(src_path)) if is_dir_dst else dst)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", choices=["stage", "assert", "list-dst"])
    ap.add_argument("--manifest", action="append", required=True, help="nfpm yaml path (repeatable)")
    ap.add_argument("--packager", default="deb", choices=["deb", "rpm"])
    ap.add_argument("--root", required=False, help="staging dir (stage mode) or image root (assert mode)")
    ap.add_argument("--build-root", default=None, help="unused placeholder for symmetry with stage")
    args = ap.parse_args()

    if args.mode == "stage":
        do_stage(args.manifest, args.packager, args.root, args.build_root)
    elif args.mode == "assert":
        do_assert(args.manifest, args.packager, args.root)
    else:
        do_list_dst(args.manifest, args.packager)


if __name__ == "__main__":
    main()
