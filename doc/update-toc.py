#!/usr/bin/env python3
"""Regenerate the table of contents of a markdown file, in place.

The TOC lives between two identical `<!-- TOC -->` markers, which is the shape
these documents already use. Everything between them is replaced; the rest of
the file is untouched.

Anchors are built the way GitHub builds them, which matters twice over: the
TOC links have to work on GitHub, and the body of these documents already
links to sections using GitHub anchors. A generator with its own convention
would silently break both.

Usage:
    ./update-toc.py FILE...          rewrite the TOC of each file
    ./update-toc.py --check FILE...  report without writing, exit 1 if stale
"""

import argparse
import io
import re
import sys

TOC_MARKER = '<!-- TOC -->'


def github_anchor(text: str, seen: dict) -> str:
    """Build the anchor GitHub would give to a heading.

    Inline markup is dropped, the text is lowercased, anything that is neither
    a word character nor a space nor a dash goes away, and spaces become
    dashes. Accents and underscores are kept -- they are word characters.
    Runs of spaces are NOT collapsed: `Vue d'ensemble : qui gère` yields
    `vue-densemble--qui-gère`, with two dashes, and a TOC that collapsed them
    would link nowhere.

    Args:
        text: the heading text, markdown included.
        seen: anchor -> how many times it has been used, updated in place.
              GitHub disambiguates repeats with `-1`, `-2`, and so on.

    Returns:
        The anchor, without its leading `#`.
    """
    s = strip_markup(text).lower()
    s = re.sub(r'[^\w\s-]', '', s, flags=re.UNICODE)
    # No strip() here: a heading ending in ` :` loses the colon but keeps the
    # space before it, which becomes a trailing dash. GitHub does the same, and
    # a TOC that trimmed it would link nowhere.
    s = re.sub(r'\s', '-', s)
    n = seen.get(s, 0)
    seen[s] = n + 1
    return s if n == 0 else f'{s}-{n}'


def strip_markup(text: str) -> str:
    """Remove the inline markdown that must not reach an anchor.

    Args:
        text: the heading text.

    Returns:
        The same text without code spans, emphasis or links.
    """
    s = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)   # links
    s = re.sub(r'`([^`]*)`', r'\1', s)                  # code spans
    s = re.sub(r'\*\*([^*]*)\*\*', r'\1', s)            # bold
    s = re.sub(r'\*([^*]*)\*', r'\1', s)                # italics
    s = s.replace('\\', '')                             # escapes
    return s.strip()


def headings(lines: list) -> list:
    """Collect the headings of a markdown document.

    Fenced blocks are skipped: these documents are full of shell and config
    snippets whose comments start with `#`, and counting those as sections is
    the classic way to produce a nonsensical TOC.

    Setext headings are recognised for `===` only (level 1). `---` is left
    alone on purpose -- it is far more often a horizontal rule here than a
    level-2 heading, and guessing wrong would inject junk into the TOC.

    Args:
        lines: the file, line by line, newlines included.

    Returns:
        A list of (level, text) in document order.
    """
    out = []
    fence = False
    for i, line in enumerate(lines):
        if line.startswith('```'):
            fence = not fence
            continue
        if fence:
            continue
        m = re.match(r'^(#{1,6}) +(.+?)\s*#*\s*$', line)
        if m:
            out.append((len(m.group(1)), m.group(2).strip()))
            continue
        # Setext level 1: a non-empty line underlined with `===`.
        if (re.match(r'^=+\s*$', line) and i > 0
                and lines[i - 1].strip()
                and not lines[i - 1].startswith('#')):
            out.append((1, lines[i - 1].strip()))
    return out


def escape_underscores(text: str) -> str:
    """Escape underscores outside code spans, the way the TOC was written.

    `broker_state` in plain text would be read as emphasis, hence the
    backslash. Inside backticks nothing is interpreted, so escaping there
    would show the backslash instead of hiding it.

    Args:
        text: the heading text.

    Returns:
        The label to display in the TOC.
    """
    parts = re.split(r'(`[^`]*`)', text)
    return ''.join(part if part.startswith('`')
                   else re.sub(r'(?<!\\)_', r'\\_', part)
                   for part in parts)


def render(heads: list) -> list:
    """Render the TOC lines for the given headings.

    Args:
        heads: (level, text) pairs.

    Returns:
        The TOC as a list of lines, without trailing newlines.
    """
    seen = {}
    out = []
    for level, text in heads:
        anchor = github_anchor(text, seen)
        label = escape_underscores(text)
        out.append(f"{'  ' * (level - 1)}* [{label}](#{anchor})")
    return out


def process(path: str, check: bool) -> bool:
    """Rewrite -- or verify -- the TOC of one file.

    Args:
        path: the markdown file.
        check: report only, do not write.

    Returns:
        True if the file is already up to date.
    """
    text = io.open(path, encoding='utf-8').read()
    parts = text.split(TOC_MARKER)
    if len(parts) != 3:
        print(f'{path}: expected exactly two {TOC_MARKER} markers, '
              f'found {len(parts) - 1}', file=sys.stderr)
        return False

    lines = text.splitlines(keepends=True)
    toc = render(headings(lines))
    rebuilt = f'{parts[0]}{TOC_MARKER}\n' + '\n'.join(toc) + f'\n{TOC_MARKER}{parts[2]}'

    if rebuilt == text:
        print(f'{path}: TOC already up to date ({len(toc)} entries)')
        return True
    if check:
        old = len([l for l in parts[1].splitlines() if l.strip().startswith('*')])
        print(f'{path}: TOC is stale ({old} entries, {len(toc)} expected)')
        return False
    io.open(path, 'w', encoding='utf-8').write(rebuilt)
    print(f'{path}: TOC rewritten ({len(toc)} entries)')
    return True


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('files', nargs='+', metavar='FILE')
    p.add_argument('--check', action='store_true',
                   help='report without writing; exit 1 if any TOC is stale')
    args = p.parse_args()
    # Every file is processed before the verdict: all() would short-circuit on
    # the first stale one and hide the state of the others.
    results = [process(f, args.check) for f in args.files]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
