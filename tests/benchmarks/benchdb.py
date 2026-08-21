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
"""The store where every benchmark result lands: one SQLite file.

Three tables, and the shape is deliberate:

* ``run`` is one measured point -- one benchmark, one variant, one size. A sweep
  over three sizes is three runs sharing a label.
* ``metric`` holds the scalars, in key/value form. That is what lets the
  allocation benchmark (alloc_calls, peak_heap_bytes) and the load benchmark
  (cpu_avg_pct, cpu_ms_per_check) share one store without a column per
  benchmark, and ``compare`` work the same way for both.
* ``series`` holds anything indexed: the RSS curve of a one-hour window as much
  as the three repetitions of a configuration load. Long format, so a new
  measurement never needs a schema change.

Access goes through Python only: the podman image carries the sqlite3 module but
no sqlite3 command line tool.
"""

import json
import os
import sqlite3
from typing import Any, Optional

SCHEMA_VERSION = 1

_SCHEMA = """
CREATE TABLE IF NOT EXISTS run (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    label        TEXT NOT NULL,
    bench        TEXT NOT NULL,
    variant      TEXT NOT NULL DEFAULT '',
    started_at   TEXT NOT NULL,
    ended_at     TEXT,
    status       TEXT NOT NULL DEFAULT 'running',
    git_branch   TEXT,
    git_commit   TEXT,
    git_dirty    INTEGER,
    host         TEXT,
    container    TEXT,
    cpu_count    INTEGER,
    params_json  TEXT NOT NULL DEFAULT '{}',
    notes        TEXT
);

CREATE TABLE IF NOT EXISTS metric (
    run_id  INTEGER NOT NULL REFERENCES run(id) ON DELETE CASCADE,
    name    TEXT NOT NULL,
    value   REAL NOT NULL,
    unit    TEXT,
    PRIMARY KEY (run_id, name)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS series (
    run_id  INTEGER NOT NULL REFERENCES run(id) ON DELETE CASCADE,
    seq     REAL NOT NULL,
    target  TEXT NOT NULL,
    name    TEXT NOT NULL,
    value   REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_run_label ON run(label, bench, variant);
CREATE INDEX IF NOT EXISTS idx_series_run ON series(run_id, target, name);
"""


class BenchDB:
    """A benchmark result store, backed by one SQLite file."""

    def __init__(self, path: str):
        """Open the store, creating it if needed.

        Args:
            path (str): path of the SQLite file. Its directory is created.
        """
        self.path = path
        parent = os.path.dirname(os.path.abspath(path))
        os.makedirs(parent, mode=0o775, exist_ok=True)
        self._db = sqlite3.connect(path)
        self._db.row_factory = sqlite3.Row
        self._db.execute("PRAGMA foreign_keys = ON")
        # A benchmark writes in bursts while a daemon it measures may be
        # hammering the same disk; WAL keeps those writes from serialising
        # against a concurrent reader looking at the results.
        self._db.execute("PRAGMA journal_mode = WAL")
        self._db.executescript(_SCHEMA)
        version = self._db.execute("PRAGMA user_version").fetchone()[0]
        if version == 0:
            self._db.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")
        elif version > SCHEMA_VERSION:
            raise RuntimeError(
                f"{path} was written by a newer benchdb (schema {version}, "
                f"this one knows {SCHEMA_VERSION})")
        self._db.commit()

    def commit(self):
        """Flush what has been written so far.

        add_metric() and add_series_point() do not commit on their own, so that a
        benchmark writing hundreds of points does not pay a transaction each
        time. Anything writing one value at a time has to commit itself.
        """
        self._db.commit()

    def close(self):
        """Commit and close the store."""
        self._db.commit()
        self._db.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def start_run(self, label: str, bench: str, started_at: str,
                  variant: str = "", params: Optional[dict] = None,
                  env: Optional[dict] = None, notes: Optional[str] = None) -> int:
        """Open a run and return its id.

        Args:
            label (str): names the campaign, usually the version or the branch.
            bench (str): benchmark name, e.g. "engine-config-load".
            started_at (str): ISO 8601 local time.
            variant (str, optional): which flavour of that benchmark, e.g.
                "legacy" or "EALLOC2". Defaults to "".
            params (dict, optional): what parameterises this point (sizes,
                durations). Stored as JSON and used to pair runs when comparing
                two labels. Defaults to None.
            env (dict, optional): environment description, as returned by
                benchenv.describe(). Defaults to None.
            notes (str, optional): free text. Defaults to None.

        Returns:
            The run id.
        """
        env = env or {}
        cur = self._db.execute(
            "INSERT INTO run (label, bench, variant, started_at, status, "
            "git_branch, git_commit, git_dirty, host, container, cpu_count, "
            "params_json, notes) "
            "VALUES (?, ?, ?, ?, 'running', ?, ?, ?, ?, ?, ?, ?, ?)",
            (label, bench, variant, started_at,
             env.get("git_branch"), env.get("git_commit"),
             1 if env.get("git_dirty") else 0,
             env.get("host"), env.get("container"), env.get("cpu_count"),
             json.dumps(params or {}, sort_keys=True), notes))
        self._db.commit()
        run_id = cur.lastrowid
        if run_id is None:
            raise RuntimeError("the store refused to create the run")
        return run_id

    def finish_run(self, run_id: int, ended_at: str, status: str = "ok"):
        """Close a run.

        Args:
            run_id (int): the run to close.
            ended_at (str): ISO 8601 local time.
            status (str, optional): "ok", "failed" or "void". Defaults to "ok".
                A run left "running" is one whose benchmark died, and it is kept
                on purpose rather than deleted: a crash is a result too.
        """
        self._db.execute("UPDATE run SET ended_at = ?, status = ? WHERE id = ?",
                         (ended_at, status, run_id))
        self._db.commit()

    def append_note(self, run_id: int, text: str):
        """Add something to the notes of a run.

        Used for what is only known once the run is over -- the path of a
        heaptrack trace, for instance, which a later comparison needs to find.

        Args:
            run_id (int): the run.
            text (str): what to append, space separated from what is there.
        """
        self._db.execute(
            "UPDATE run SET notes = TRIM(COALESCE(notes || ' ', '') || ?) "
            "WHERE id = ?", (text, run_id))
        self._db.commit()

    def add_metric(self, run_id: int, name: str, value: float,
                   unit: Optional[str] = None):
        """Store one scalar result.

        Args:
            run_id (int): the run it belongs to.
            name (str): metric name, e.g. "cpu_total_s".
            value (float): its value.
            unit (str, optional): unit, for display only. Defaults to None.
        """
        self._db.execute(
            "INSERT OR REPLACE INTO metric (run_id, name, value, unit) "
            "VALUES (?, ?, ?, ?)", (run_id, name, float(value), unit))

    def add_metrics(self, run_id: int, metrics: dict[str, Any],
                    units: Optional[dict[str, str]] = None):
        """Store several scalar results at once.

        Args:
            run_id (int): the run they belong to.
            metrics (dict): name to value.
            units (dict, optional): name to unit. Defaults to None.
        """
        units = units or {}
        for name, value in metrics.items():
            self.add_metric(run_id, name, value, units.get(name))
        self._db.commit()

    def add_series_point(self, run_id: int, seq: float, target: str,
                         name: str, value: float):
        """Store one point of an indexed measurement.

        Args:
            run_id (int): the run it belongs to.
            seq (float): the index -- elapsed seconds for a window, iteration
                number for a repeated measurement.
            target (str): what was measured, e.g. "centengine" or "machine".
            name (str): the quantity, e.g. "rss_kb".
            value (float): its value.
        """
        self._db.execute(
            "INSERT INTO series (run_id, seq, target, name, value) "
            "VALUES (?, ?, ?, ?, ?)", (run_id, seq, target, name, float(value)))

    def add_series(self, run_id: int, seq: float, target: str, values: dict):
        """Store several quantities measured at the same index.

        Args:
            run_id (int): the run they belong to.
            seq (float): the index.
            target (str): what was measured.
            values (dict): quantity name to value.
        """
        for name, value in values.items():
            self.add_series_point(run_id, seq, target, name, value)
        self._db.commit()

    def runs(self, label: Optional[str] = None, bench: Optional[str] = None,
             limit: int = 0) -> list[sqlite3.Row]:
        """List runs, newest first.

        Args:
            label (str, optional): keep only that label. Defaults to None.
            bench (str, optional): keep only that benchmark. Defaults to None.
            limit (int, optional): at most that many. Defaults to 0, no limit.

        Returns:
            The matching rows of the run table.
        """
        query = "SELECT * FROM run"
        where, args = [], []
        if label:
            where.append("label = ?")
            args.append(label)
        if bench:
            where.append("bench = ?")
            args.append(bench)
        if where:
            query += " WHERE " + " AND ".join(where)
        query += " ORDER BY id DESC"
        if limit:
            query += f" LIMIT {int(limit)}"
        return self._db.execute(query, args).fetchall()

    def run(self, run_id: int) -> Optional[sqlite3.Row]:
        """Return one run by id.

        Args:
            run_id (int): the run to fetch.

        Returns:
            Its row, or None.
        """
        return self._db.execute("SELECT * FROM run WHERE id = ?",
                                (run_id,)).fetchone()

    def labels(self) -> list[sqlite3.Row]:
        """List the campaigns held by the store.

        Returns:
            One row per label with its benchmarks, run count and last date.
        """
        return self._db.execute(
            "SELECT label, COUNT(*) AS runs, MAX(started_at) AS last, "
            "GROUP_CONCAT(DISTINCT bench) AS benches "
            "FROM run GROUP BY label ORDER BY last DESC").fetchall()

    def metrics(self, run_id: int) -> dict[str, tuple[float, Optional[str]]]:
        """Return every scalar of a run.

        Args:
            run_id (int): the run to read.

        Returns:
            name to (value, unit).
        """
        rows = self._db.execute(
            "SELECT name, value, unit FROM metric WHERE run_id = ? "
            "ORDER BY name", (run_id,)).fetchall()
        return {r["name"]: (r["value"], r["unit"]) for r in rows}

    def series(self, run_id: int, target: Optional[str] = None,
               name: Optional[str] = None) -> list[sqlite3.Row]:
        """Return the indexed measurements of a run.

        Args:
            run_id (int): the run to read.
            target (str, optional): keep only that target. Defaults to None.
            name (str, optional): keep only that quantity. Defaults to None.

        Returns:
            The matching rows, ordered by index.
        """
        query = "SELECT seq, target, name, value FROM series WHERE run_id = ?"
        args: list = [run_id]
        if target:
            query += " AND target = ?"
            args.append(target)
        if name:
            query += " AND name = ?"
            args.append(name)
        query += " ORDER BY seq, target, name"
        return self._db.execute(query, args).fetchall()

    def pair_runs(self, label_a: str, label_b: str,
                  bench: Optional[str] = None) -> list[tuple[sqlite3.Row, sqlite3.Row]]:
        """Pair the runs of two campaigns that measured the same thing.

        Two runs are the same measurement when their benchmark, variant and
        parameters match; params_json is stored with sorted keys precisely so
        that this comparison is a string equality. When a label holds several
        runs of the same point -- a benchmark run twice -- the most recent one
        wins, which is what someone re-running a point after a fix expects.

        Args:
            label_a (str): the reference campaign.
            label_b (str): the campaign to compare to it.
            bench (str, optional): restrict to one benchmark. Defaults to None.

        Returns:
            A list of (run_a, run_b), ordered by benchmark then variant.
        """
        def by_key(label):
            out = {}
            for row in self.runs(label=label, bench=bench):
                key = (row["bench"], row["variant"], row["params_json"])
                # runs() is newest first, so the first one seen is the one kept.
                out.setdefault(key, row)
            return out

        a, b = by_key(label_a), by_key(label_b)
        keys = sorted(set(a) & set(b))
        return [(a[k], b[k]) for k in keys]
