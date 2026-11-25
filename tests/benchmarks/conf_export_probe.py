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
"""The real-time side of the conf-export benchmark: a load, and a latency probe.

While a configuration export runs, the question is whether the events of the
pollers keep flowing through the central cbd. Two things run here, side by side,
for as long as the benchmark needs them:

  - a passive load, at a fixed rate spread over every poller, so that each
    stream of the central cbd is reading all the time, as on a real platform;
  - a probe, which flips the notifications of a few services of each poller
    with external commands, and times how long each change takes to reach the
    database.

The probe goes through the adaptive path on purpose. A full status reaches the
database through the bulk binds of unified_sql, flushed every thirty seconds or
so: a stream stalled for a few seconds would vanish in that delay. An adaptive
service is written by a direct query as soon as unified_sql reads it, so its
latency is that of the chain itself -- Engine, the stream of the central cbd,
the multiplexing, unified_sql -- and a stalled stream shows up in it whole.

It runs as a separate process, not as a robot keyword, so that robot can drive
the export meanwhile. It stops on SIGTERM or SIGINT, and writes its samples out
then: one JSON document, read back by ctn_bench_conf_export_summary().
"""

import argparse
import json
import os
import signal
import sys
import threading
import time

import pymysql

VAR_ROOT = "/tmp/var"
# How many services of each poller the probe flips in turn.
PROBES_BY_POLLER = 9


def command_file(poller: int) -> str:
    """Return the external command FIFO of a poller.

    Args:
        poller (int): the poller index, from 0.

    Returns:
        The path of the FIFO.
    """
    return f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd"


def write_commands(poller: int, lines: list):
    """Write external commands into the FIFO of a poller.

    One write per line: a write of at most PIPE_BUF bytes into a FIFO is atomic,
    so the load and the probe can share it without interleaving their lines.

    Args:
        poller (int): the poller index, from 0.
        lines (list): the commands, each one ending with a newline.
    """
    fd = os.open(command_file(poller), os.O_WRONLY)
    try:
        for line in lines:
            os.write(fd, line.encode())
    finally:
        os.close(fd)


class Layout:
    """Where the hosts and services of each poller are, in the generated
    configuration.

    Ctn Config Centralized Engine hands the hosts out in order, the same number
    to every poller when the count divides evenly, and numbers the services
    globally: host h owns services (h - 1) * S + 1 to h * S.
    """

    def __init__(self, pollers: int, hosts: int, services_by_host: int):
        if hosts % pollers:
            raise ValueError(
                f"{hosts} hosts do not divide evenly between {pollers} pollers")
        self.pollers = pollers
        self.hosts_by_poller = hosts // pollers
        self.services_by_host = services_by_host

    def first_host(self, poller: int) -> int:
        """Return the first host of a poller."""
        return poller * self.hosts_by_poller + 1

    def service(self, host: int, index: int) -> int:
        """Return the global number of the index-th service of a host."""
        return (host - 1) * self.services_by_host + index

    def probe_services(self, poller: int) -> list:
        """Return the (host, service) pairs the probe flips on a poller.

        The services 1 to 9 of the first host of the poller: their numbers
        never end with 0, so the exports, which change the services whose
        number ends with 0, never rewrite them -- a configuration diff resetting
        their notifications would read as a flip that never arrives.
        """
        host = self.first_host(poller)
        count = min(PROBES_BY_POLLER, self.services_by_host)
        return [(host, self.service(host, i)) for i in range(1, count + 1)
                if self.service(host, i) % 10]


def sustain_load(layout: Layout, rate: int, output: str,
                 stop: threading.Event, counter: list):
    """Submit passive results at a steady rate until told to stop.

    Args:
        layout (Layout): the configuration.
        rate (int): results per second, for the whole platform.
        output (str): the check output.
        stop (threading.Event): set when the load has to stop.
        counter (list): one integer, the results submitted so far.
    """
    per_poller = max(1, rate // layout.pollers)
    services = layout.hosts_by_poller * layout.services_by_host
    slot = 0
    while not stop.is_set():
        started = time.monotonic()
        now = int(time.time())
        for poller in range(layout.pollers):
            lines = []
            for i in range(per_poller):
                k = (slot + i) % services
                host = layout.first_host(poller) + k // layout.services_by_host
                service = layout.service(host, k % layout.services_by_host + 1)
                lines.append(f"[{now}] PROCESS_SERVICE_CHECK_RESULT;host_{host};"
                             f"service_{service};0;{output}\n")
            write_commands(poller, lines)
            counter[0] += len(lines)
        slot += per_poller
        stop.wait(max(0.0, 1.0 - (time.monotonic() - started)))


def read_notify(cursor, pairs: list) -> dict:
    """Read the notify column of some services.

    Args:
        cursor: a cursor on the storage database.
        pairs (list): (host_id, service_id) pairs.

    Returns:
        (host_id, service_id) to notify value, for the services the table holds.
    """
    if not pairs:
        return {}
    cond = " OR ".join(["(host_id=%s AND service_id=%s)"] * len(pairs))
    cursor.execute(f"SELECT host_id, service_id, notify FROM services "
                   f"WHERE {cond}", [v for p in pairs for v in p])
    return {(row[0], row[1]): row[2] for row in cursor.fetchall()}


def probe(layout: Layout, period: float, timeout: float, db: dict,
          stop: threading.Event, samples: list):
    """Time the notification flips until told to stop.

    Every period, one service of each poller is flipped -- the next one of its
    probed services that has no flip in flight -- without waiting for the
    previous flips to show. The latency of the adaptive path is about a second,
    so waiting for each flip before sending the next one would sample once a
    second, and an export shorter than that would go unseen. The database is
    polled meanwhile for every flip in flight at once.

    A sample is the sending time and the latency of one flip. A flip that never
    showed is kept with a latency of None: a stream stalled beyond the timeout
    is the very thing being looked for.

    Args:
        layout (Layout): the configuration.
        period (float): seconds between two sendings.
        timeout (float): seconds after which a flip is given up.
        db (dict): connection parameters of the storage database.
        stop (threading.Event): set when the probe has to stop.
        samples (list): where the samples go, as dictionaries.
    """
    probed = {p: layout.probe_services(p) for p in range(layout.pollers)}
    every = [pair for pairs in probed.values() for pair in pairs]
    # (host, service) to (poller, wanted value, sending time).
    in_flight = {}
    turn = {p: 0 for p in probed}
    conn = pymysql.connect(autocommit=True, **db)
    try:
        with conn.cursor() as cursor:
            # The rows are written by the first global diff: until then there
            # is nothing to flip.
            current = read_notify(cursor, every)
            while len(current) < len(every) and not stop.is_set():
                stop.wait(1.0)
                current = read_notify(cursor, every)
            next_send = time.monotonic()
            while not stop.is_set():
                if time.monotonic() >= next_send:
                    next_send += period
                    for p, pairs in probed.items():
                        for _ in range(len(pairs)):
                            pair = pairs[turn[p] % len(pairs)]
                            turn[p] += 1
                            if pair not in in_flight:
                                break
                        else:
                            continue
                        wanted = 0 if current[pair] else 1
                        cmd = ("ENABLE_SVC_NOTIFICATIONS" if wanted
                               else "DISABLE_SVC_NOTIFICATIONS")
                        sent = time.time()
                        write_commands(p, [f"[{int(sent)}] {cmd};"
                                           f"host_{pair[0]};service_{pair[1]}\n"])
                        in_flight[pair] = (p, wanted, sent)
                seen = read_notify(cursor, list(in_flight))
                now = time.time()
                for pair, (p, wanted, sent) in list(in_flight.items()):
                    if seen.get(pair) == wanted:
                        samples.append({"poller": p, "service": pair[1],
                                        "wanted": wanted, "sent": sent,
                                        "latency": now - sent})
                        current[pair] = wanted
                        del in_flight[pair]
                    elif now - sent > timeout:
                        samples.append({"poller": p, "service": pair[1],
                                        "wanted": wanted, "sent": sent,
                                        "latency": None})
                        current[pair] = seen.get(pair, current[pair])
                        del in_flight[pair]
                stop.wait(0.01)
    finally:
        conn.close()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="The real-time side of the conf-export benchmark.")
    parser.add_argument("--pollers", type=int, required=True)
    parser.add_argument("--hosts", type=int, required=True,
                        help="hosts of the whole platform")
    parser.add_argument("--services-by-host", type=int, required=True)
    parser.add_argument("--rate", type=int, default=200,
                        help="passive results per second, whole platform; "
                        "0 for no load")
    parser.add_argument("--period", type=float, default=0.2,
                        help="seconds between two flips on each poller")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="seconds after which a flip is given up")
    parser.add_argument("--output", default="OK - CPU usage is 12 percent | "
                        "cpu=12%;80;90;0;100 mem=45%;70;85;0;100")
    parser.add_argument("--out", required=True,
                        help="the JSON file the samples are written to")
    parser.add_argument("--db-host", default="localhost")
    parser.add_argument("--db-port", type=int, default=3306)
    parser.add_argument("--db-user", default="centreon")
    parser.add_argument("--db-password", default="centreon")
    parser.add_argument("--db-name", default="centreon_storage")
    args = parser.parse_args()

    layout = Layout(args.pollers, args.hosts, args.services_by_host)
    db = {"host": args.db_host, "port": args.db_port, "user": args.db_user,
          "password": args.db_password, "database": args.db_name}
    stop = threading.Event()
    samples = []
    submitted = [0]

    def on_signal(_signum, _frame):
        stop.set()

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)

    threads = [threading.Thread(target=probe, daemon=True,
                                args=(layout, args.period, args.timeout, db,
                                      stop, samples))]
    if args.rate > 0:
        threads.append(threading.Thread(target=sustain_load, daemon=True,
                                        args=(layout, args.rate, args.output,
                                              stop, submitted)))
    for t in threads:
        t.start()
    while not stop.is_set():
        stop.wait(0.5)
        if not threads[0].is_alive():
            # The probe died -- the database went away, say: nothing left to
            # measure, and the samples so far are still worth writing.
            print("conf_export_probe: the probe stopped on its own",
                  file=sys.stderr, flush=True)
            stop.set()
    for t in threads:
        t.join(timeout=args.timeout + 5)

    tmp = f"{args.out}.tmp"
    with open(tmp, "w") as f:
        json.dump({"samples": samples, "submitted": submitted[0]}, f)
    os.replace(tmp, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
